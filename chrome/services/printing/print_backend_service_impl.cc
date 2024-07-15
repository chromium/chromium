// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/printing/print_backend_service_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/printing/printing_init.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "components/crash/core/common/crash_keys.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "printing/backend/print_backend.h"
#include "printing/buildflags/buildflags.h"
#include "printing/metafile.h"
#include "printing/metafile_skia.h"
#include "printing/mojom/print.mojom.h"
#include "printing/printed_document.h"
#include "printing/printing_context.h"

#if BUILDFLAG(IS_MAC)
#include "base/threading/thread_restrictions.h"
#include "chrome/common/printing/printer_capabilities_mac.h"
#endif

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CUPS)
#include "printing/backend/cups_connection_pool.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "base/no_destructor.h"
#include "ui/linux/linux_ui.h"
#include "ui/linux/linux_ui_delegate_stub.h"
#include "ui/linux/linux_ui_factory.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/containers/queue.h"
#include "base/types/expected.h"
#include "base/win/win_util.h"
#include "chrome/services/printing/public/mojom/printer_xml_parser.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "printing/backend/xps_utils_win.h"
#include "printing/emf_win.h"
#include "printing/printed_page_win.h"
#include "printing/printing_features.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#endif

namespace printing {

namespace {

#if BUILDFLAG(IS_LINUX)
void InstantiateLinuxUiDelegate() {
  // TODO(crbug.com/40561724)  Until a real UI can be used in a utility process,
  // need to use the stub version.
  static base::NoDestructor<ui::LinuxUiDelegateStub> linux_ui_delegate;
}
#endif

scoped_refptr<base::SequencedTaskRunner> GetPrintingTaskRunner() {
#if BUILDFLAG(IS_LINUX)
  // Use task runner associated with equivalent of UI thread.  Needed for calls
  // made through `PrintDialogLinuxInterface` to properly execute.
  CHECK(base::SequencedTaskRunner::HasCurrentDefault());
  return base::SequencedTaskRunner::GetCurrentDefault();
#else

  static constexpr base::TaskTraits kTraits = {
      base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

#if BUILDFLAG(USE_CUPS)
  // CUPS is thread safe, so a task runner can be allocated for each job.
  return base::ThreadPool::CreateSequencedTaskRunner(kTraits);
#elif BUILDFLAG(IS_WIN)
  // For Windows, we want a single threaded task runner shared for all print
  // jobs in the process because Windows printer drivers are oftentimes not
  // thread-safe.  This protects against multiple print jobs to the same device
  // from running in the driver at the same time.
  return base::ThreadPool::CreateSingleThreadTaskRunner(kTraits);
#else
  // Be conservative for unsupported platforms, use a single threaded runner
  // so that concurrent print jobs are not in driver code at the same time.
  return base::ThreadPool::CreateSingleThreadTaskRunner(kTraits);
#endif
#endif  // BUILDFLAG(IS_LINUX)
}

std::unique_ptr<Metafile> CreateMetafile(mojom::MetafileDataType data_type) {
  switch (data_type) {
    case mojom::MetafileDataType::kPDF:
      return std::make_unique<MetafileSkia>();
#if BUILDFLAG(IS_WIN)
    case mojom::MetafileDataType::kEMF:
      return std::make_unique<Emf>();
    case mojom::MetafileDataType::kPostScriptEmf:
      return std::make_unique<PostScriptMetaFile>();
#endif
  }
}

struct RenderData {
  base::HeapArray<uint8_t> data_copy;
  std::unique_ptr<Metafile> metafile;
};

std::optional<RenderData> PrepareRenderData(
    int document_cookie,
    mojom::MetafileDataType page_data_type,
    const base::ReadOnlySharedMemoryRegion& serialized_data) {
  base::ReadOnlySharedMemoryMapping mapping = serialized_data.Map();
  if (!mapping.IsValid()) {
    DLOG(ERROR) << "Failure printing document " << document_cookie
                << ", cannot map input.";
    return std::nullopt;
  }

  RenderData render_data;
  render_data.metafile = CreateMetafile(page_data_type);

  // For security reasons we need to use a copy of the data, and not operate
  // on it directly out of shared memory.  Make a copy here if the underlying
  // `Metafile` implementation doesn't do it automatically.
  // TODO(crbug.com/40151989)  Eliminate this copy when the shared memory can't
  // be written by the sender.
  base::span<const uint8_t> data = mapping.GetMemoryAsSpan<uint8_t>();
  if (render_data.metafile->ShouldCopySharedMemoryRegionData()) {
    render_data.data_copy = base::HeapArray<uint8_t>::Uninit(data.size());
    render_data.data_copy.as_span().copy_from(data);
    data = render_data.data_copy.as_span();
  }
  if (!render_data.metafile->InitFromData(data)) {
    DLOG(ERROR) << "Failure printing document " << document_cookie
                << ", unable to initialize.";
    return std::nullopt;
  }
  return render_data;
}

// Local storage of document and associated data needed to submit to job to
// the operating system's printing API.  All access to the document occurs on
// a worker task runner.
class DocumentContainer {
 public:
  DocumentContainer(std::unique_ptr<PrintingContext::Delegate> context_delegate,
                    std::unique_ptr<PrintingContext> context,
                    scoped_refptr<PrintedDocument> document)
      : context_delegate_(std::move(context_delegate)),
        context_(std::move(context)),
        document_(document) {}

  ~DocumentContainer() = default;

  // Helper functions that runs on a task runner.
  PrintBackendServiceImpl::StartPrintingResult StartPrintingReadyDocument();
#if BUILDFLAG(IS_WIN)
  mojom::ResultCode DoRenderPrintedPage(
      uint32_t page_index,
      mojom::MetafileDataType page_data_type,
      base::ReadOnlySharedMemoryRegion serialized_page,
      gfx::Size page_size,
      gfx::Rect page_content_rect,
      float shrink_factor);
#endif
  mojom::ResultCode DoRenderPrintedDocument(
      uint32_t page_count,
      mojom::MetafileDataType data_type,
      base::ReadOnlySharedMemoryRegion serialized_document);
  mojom::ResultCode DoDocumentDone();
  void DoCancel();

 private:
  std::unique_ptr<PrintingContext::Delegate> context_delegate_;
  std::unique_ptr<PrintingContext> context_;

  scoped_refptr<PrintedDocument> document_;

  // Ensure all interactions for this document are issued from the same runner.
  SEQUENCE_CHECKER(sequence_checker_);
};

PrintBackendServiceImpl::StartPrintingResult
DocumentContainer::StartPrintingReadyDocument() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOG(1) << "Start printing for document " << document_->cookie();

  PrintBackendServiceImpl::StartPrintingResult printing_result;
  printing_result.result = context_->NewDocument(document_->name());
  if (printing_result.result != mojom::ResultCode::kSuccess) {
    DLOG(ERROR) << "Failure initializing new document " << document_->cookie()
                << ", error: " << printing_result.result;
    context_->Cancel();
    printing_result.job_id = PrintingContext::kNoPrintJobId;
    return printing_result;
  }
  printing_result.job_id = context_->job_id();

  return printing_result;
}

#if BUILDFLAG(IS_WIN)
mojom::ResultCode DocumentContainer::DoRenderPrintedPage(
    uint32_t page_index,
    mojom::MetafileDataType page_data_type,
    base::ReadOnlySharedMemoryRegion serialized_page,
    gfx::Size page_size,
    gfx::Rect page_content_rect,
    float shrink_factor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOG(1) << "Render printed page " << page_index << " for document "
           << document_->cookie();

  std::optional<RenderData> render_data =
      PrepareRenderData(document_->cookie(), page_data_type, serialized_page);
  if (!render_data) {
    DLOG(ERROR) << "Failure preparing render data for document "
                << document_->cookie();
    context_->Cancel();
    return mojom::ResultCode::kFailed;
  }

  document_->SetPage(page_index, std::move(render_data->metafile),
                     shrink_factor, page_size, page_content_rect);

  mojom::ResultCode result = document_->RenderPrintedPage(
      *document_->GetPage(page_index), context_.get());
  if (result != mojom::ResultCode::kSuccess) {
    DLOG(ERROR) << "Failure rendering page " << page_index << " of document "
                << document_->cookie() << ", error: " << result;
    context_->Cancel();
  }
  return result;
}
#endif  // BUILDFLAG(IS_WIN)

mojom::ResultCode DocumentContainer::DoRenderPrintedDocument(
    uint32_t page_count,
    mojom::MetafileDataType data_type,
    base::ReadOnlySharedMemoryRegion serialized_document) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOG(1) << "Render printed document " << document_->cookie();

  std::optional<RenderData> render_data =
      PrepareRenderData(document_->cookie(), data_type, serialized_document);
  if (!render_data) {
    DLOG(ERROR) << "Failure preparing render data for document "
                << document_->cookie();
    context_->Cancel();
    return mojom::ResultCode::kFailed;
  }

  document_->set_page_count(page_count);
  document_->SetDocument(std::move(render_data->metafile));

  mojom::ResultCode result = document_->RenderPrintedDocument(context_.get());
  if (result != mojom::ResultCode::kSuccess) {
    DLOG(ERROR) << "Failure rendering document " << document_->cookie()
                << ", error: " << result;
    context_->Cancel();
  }
  return result;
}

mojom::ResultCode DocumentContainer::DoDocumentDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOG(1) << "Document done for document " << document_->cookie();
  mojom::ResultCode result = context_->DocumentDone();
  if (result != mojom::ResultCode::kSuccess) {
    DLOG(ERROR) << "Failure completing document " << document_->cookie()
                << ", error: " << result;
    context_->Cancel();
  }
  return result;
}

void DocumentContainer::DoCancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOG(1) << "Canceling document " << document_->cookie();
  context_->Cancel();
}

}  // namespace

// Helper for managing `DocumentContainer` objects.  All access to this occurs
// on the main thread.
class PrintBackendServiceImpl::DocumentHelper {
 public:
  DocumentHelper(
      int document_cookie,
      base::SequenceBound<DocumentContainer> document_container,
      mojom::PrintBackendService::StartPrintingCallback start_printing_callback)
      : document_cookie_(document_cookie),
        document_container_(std::move(document_container)),
        start_printing_callback_(std::move(start_printing_callback)) {}

  ~DocumentHelper() = default;

  int document_cookie() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return document_cookie_;
  }

  base::SequenceBound<DocumentContainer>& document_container() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return document_container_;
  }

  mojom::PrintBackendService::StartPrintingCallback
  TakeStartPrintingCallback() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return std::move(start_printing_callback_);
  }

 private:
  const int document_cookie_;

  base::SequenceBound<DocumentContainer> document_container_;

  // `start_printing_callback_` is held until the document is ready for
  // printing.
  mojom::PrintBackendService::StartPrintingCallback start_printing_callback_;

  // Ensure all interactions for this document are issued from the same runner.
  SEQUENCE_CHECKER(sequence_checker_);
};

// Sandboxed service helper.
SandboxedPrintBackendHostImpl::SandboxedPrintBackendHostImpl(
    mojo::PendingReceiver<mojom::SandboxedPrintBackendHost> receiver)
    : receiver_(this, std::move(receiver)) {}

SandboxedPrintBackendHostImpl::~SandboxedPrintBackendHostImpl() = default;

void SandboxedPrintBackendHostImpl::BindBackend(
    mojo::PendingReceiver<mojom::PrintBackendService> receiver) {
  CHECK(!print_backend_service_)
      << "Cannot bind service twice in same process.";
  print_backend_service_ =
      std::make_unique<PrintBackendServiceImpl>(std::move(receiver));
}

// Unsandboxed service helper.
UnsandboxedPrintBackendHostImpl::UnsandboxedPrintBackendHostImpl(
    mojo::PendingReceiver<mojom::UnsandboxedPrintBackendHost> receiver)
    : receiver_(this, std::move(receiver)) {}

UnsandboxedPrintBackendHostImpl::~UnsandboxedPrintBackendHostImpl() = default;

void UnsandboxedPrintBackendHostImpl::BindBackend(
    mojo::PendingReceiver<mojom::PrintBackendService> receiver) {
  CHECK(!print_backend_service_)
      << "Cannot bind service twice in same process.";
  print_backend_service_ =
      std::make_unique<PrintBackendServiceImpl>(std::move(receiver));
}

PrintBackendServiceImpl::PrintingContextDelegate::PrintingContextDelegate() =
    default;
PrintBackendServiceImpl::PrintingContextDelegate::~PrintingContextDelegate() =
    default;

gfx::NativeView
PrintBackendServiceImpl::PrintingContextDelegate::GetParentView() {
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  return parent_native_view_;
#else
  NOTREACHED_IN_MIGRATION();
  return nullptr;
#endif
}

std::string PrintBackendServiceImpl::PrintingContextDelegate::GetAppLocale() {
  return locale_;
}

#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
void PrintBackendServiceImpl::PrintingContextDelegate::SetParentWindow(
    uint32_t parent_window_id) {
#if BUILDFLAG(IS_WIN)
  parent_native_view_ = reinterpret_cast<gfx::NativeView>(
      base::win::Uint32ToHandle(parent_window_id));
#else
  NOTREACHED_IN_MIGRATION();
#endif
}
#endif

void PrintBackendServiceImpl::PrintingContextDelegate::SetAppLocale(
    const std::string& locale) {
  locale_ = locale;
}

// Holds the context and associated delegate for persistent usage across
// multiple settings calls until they are ready to be used to print a
// document.  Required since `PrintingContext` does not own the corresponding
// delegate object that it relies upon.
struct PrintBackendServiceImpl::ContextContainer {
  ContextContainer() = default;
  ~ContextContainer() = default;

  std::unique_ptr<PrintingContextDelegate> delegate;
  std::unique_ptr<PrintingContext> context;
};

PrintBackendServiceImpl::PrintBackendServiceImpl(
    mojo::PendingReceiver<mojom::PrintBackendService> receiver)
    : receiver_(this, std::move(receiver)) {}

PrintBackendServiceImpl::~PrintBackendServiceImpl() = default;

void PrintBackendServiceImpl::InitCommon(
#if BUILDFLAG(IS_WIN)
    const std::string& locale,
    mojo::PendingRemote<mojom::PrinterXmlParser> remote
#else
    const std::string& locale
#endif  // BUILDFLAG(IS_WIN)
) {
  locale_ = locale;
#if BUILDFLAG(IS_WIN)
  if (remote.is_valid())
    xml_parser_remote_.Bind(std::move(remote));
#endif  // BUILDFLAG(IS_WIN)
}

void PrintBackendServiceImpl::Init(
#if BUILDFLAG(IS_WIN)
    const std::string& locale,
    mojo::PendingRemote<mojom::PrinterXmlParser> remote
#else
    const std::string& locale
#endif  // BUILDFLAG(IS_WIN)
) {
  // Test classes should not invoke this base initialization method, as process
  // initialization is very different for test frameworks.  Test classes
  // will also provide their own test version of a `PrintBackend`.
  // Common initialization for production and testing should instead reside in
  // `InitCommon()`.
  InitializeProcessForPrinting();
  print_backend_ = PrintBackend::CreateInstance(locale);
#if BUILDFLAG(IS_LINUX)
  // Test framework already initializes the UI, so this should not go in
  // `InitCommon()`.  Additionally, low-level Linux UI is not needed when tests
  // are using `TestPrintingContext`.
  InstantiateLinuxUiDelegate();
  ui::LinuxUi::SetInstance(ui::GetDefaultLinuxUi());
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN)
  InitCommon(locale, std::move(remote));
#else
  InitCommon(locale);
#endif  // BUILDFLAG(IS_WIN)
}

// TODO(crbug.com/40775634)  Do nothing, this is just to assist an idle timeout
// change by providing a low-cost call to ensure it is applied.
void PrintBackendServiceImpl::Poke() {}

void PrintBackendServiceImpl::EnumeratePrinters(
    mojom::PrintBackendService::EnumeratePrintersCallback callback) {
  DCHECK(print_backend_);
  PrinterList printer_list;
  mojom::ResultCode result = print_backend_->EnumeratePrinters(printer_list);
  if (result != mojom::ResultCode::kSuccess) {
    std::move(callback).Run(mojom::PrinterListResult::NewResultCode(result));
    return;
  }
  std::move(callback).Run(
      mojom::PrinterListResult::NewPrinterList(std::move(printer_list)));
}

void PrintBackendServiceImpl::GetDefaultPrinterName(
    mojom::PrintBackendService::GetDefaultPrinterNameCallback callback) {
  DCHECK(print_backend_);
  std::string default_printer;
  mojom::ResultCode result =
      print_backend_->GetDefaultPrinterName(default_printer);
  if (result != mojom::ResultCode::kSuccess) {
    std::move(callback).Run(
        mojom::DefaultPrinterNameResult::NewResultCode(result));
    return;
  }
  std::move(callback).Run(
      mojom::DefaultPrinterNameResult::NewDefaultPrinterName(default_printer));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void PrintBackendServiceImpl::GetPrinterSemanticCapsAndDefaults(
    const std::string& printer_name,
    mojom::PrintBackendService::GetPrinterSemanticCapsAndDefaultsCallback
        callback) {
  DCHECK(print_backend_);
  crash_keys_ = std::make_unique<crash_keys::ScopedPrinterInfo>(
      printer_name, print_backend_->GetPrinterDriverInfo(printer_name));

  PrinterSemanticCapsAndDefaults printer_caps;
  const mojom::ResultCode result =
      print_backend_->GetPrinterSemanticCapsAndDefaults(printer_name,
                                                        &printer_caps);
  if (result != mojom::ResultCode::kSuccess) {
    std::move(callback).Run(
        mojom::PrinterSemanticCapsAndDefaultsResult::NewResultCode(result));
    return;
  }
  std::move(callback).Run(
      mojom::PrinterSemanticCapsAndDefaultsResult::NewPrinterCaps(
          std::move(printer_caps)));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void PrintBackendServiceImpl::FetchCapabilities(
    const std::string& printer_name,
    mojom::PrintBackendService::FetchCapabilitiesCallback callback) {
  DCHECK(print_backend_);
  crash_keys_ = std::make_unique<crash_keys::ScopedPrinterInfo>(
      printer_name, print_backend_->GetPrinterDriverInfo(printer_name));

  PrinterBasicInfo printer_info;
  mojom::ResultCode result =
      print_backend_->GetPrinterBasicInfo(printer_name, &printer_info);
  if (result != mojom::ResultCode::kSuccess) {
    std::move(callback).Run(
        mojom::PrinterCapsAndInfoResult::NewResultCode(result));
    return;
  }

  PrinterSemanticCapsAndDefaults caps;
  result =
      print_backend_->GetPrinterSemanticCapsAndDefaults(printer_name, &caps);
  if (result != mojom::ResultCode::kSuccess) {
    std::move(callback).Run(
        mojom::PrinterCapsAndInfoResult::NewResultCode(result));
    return;
  }

#if BUILDFLAG(IS_WIN)
  if (xml_parser_remote_.is_bound() &&
      base::FeatureList::IsEnabled(features::kReadPrinterCapabilitiesWithXps)) {
    ASSIGN_OR_RETURN(
        XpsCapabilities xps_capabilities, GetXpsCapabilities(printer_name),
        [&](mojom::ResultCode error) {
          return std::move(callback).Run(
              mojom::PrinterCapsAndInfoResult::NewResultCode(error));
        });

    MergeXpsCapabilities(std::move(xps_capabilities), caps);
  }
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
  {
    // Blocking is needed here for when macOS reads paper sizes from file.
    //
    // Fetching capabilities in the browser process happens from the thread
    // pool with the MayBlock() trait for macOS.  However this call can also
    // run from a utility process's main thread where blocking is not
    // implicitly allowed.  In order to preserve ordering, the utility process
    // must process this synchronously by blocking.
    //
    // TODO(crbug.com/40163200):  Investigate whether utility process main
    // thread should be allowed to block like in-process workers are.
    base::ScopedAllowBlocking allow_blocking;
    caps.user_defined_papers = GetMacCustomPaperSizes();
  }
#endif

  mojom::PrinterCapsAndInfoPtr caps_and_info =
      mojom::PrinterCapsAndInfo::New(std::move(printer_info), std::move(caps));
  std::move(callback).Run(
      mojom::PrinterCapsAndInfoResult::NewPrinterCapsAndInfo(
          std::move(caps_and_info)));
}

#if BUILDFLAG(IS_WIN)
void PrintBackendServiceImpl::GetPaperPrintableArea(
    const std::string& printer_name,
    const PrintSettings::RequestedMedia& media,
    mojom::PrintBackendService::GetPaperPrintableAreaCallback callback) {
  CHECK(print_backend_);
  crash_keys_ = std::make_unique<crash_keys::ScopedPrinterInfo>(
      printer_name, print_backend_->GetPrinterDriverInfo(printer_name));

  std::optional<gfx::Rect> printable_area_um =
      print_backend_->GetPaperPrintableArea(printer_name, media.vendor_id,
                                            media.size_microns);
  std::move(callback).Run(printable_area_um.value_or(gfx::Rect()));
}
#endif

void PrintBackendServiceImpl::EstablishPrintingContext(uint32_t context_id
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
                                                       ,
                                                       uint32_t parent_window_id
#endif
) {
  auto context_container = std::make_unique<ContextContainer>();

  context_container->delegate = CreatePrintingContextDelegate();
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  context_container->delegate->SetParentWindow(parent_window_id);
#endif

  context_container->context = PrintingContext::Create(
      context_container->delegate.get(),
      PrintingContext::ProcessBehavior::kOopEnabledPerformSystemCalls);

  bool inserted = persistent_printing_contexts_
                      .insert({context_id, std::move(context_container)})
                      .second;
  DCHECK(inserted);
}

void PrintBackendServiceImpl::UseDefaultSettings(
    uint32_t context_id,
    mojom::PrintBackendService::UseDefaultSettingsCallback callback) {
  PrintingContext* context = GetPrintingContext(context_id);
  CHECK(context) << "No context found for id " << context_id;
  mojom::ResultCode result = context->UseDefaultSettings();
  if (result != mojom::ResultCode::kSuccess) {
    DLOG(ERROR) << "Failure getting default settings of default printer, "
                << "error: " << result;
    persistent_printing_contexts_.erase(context_id);
    std::move(callback).Run(mojom::PrintSettingsResult::NewResultCode(result));
    return;
  }
  std::move(callback).Run(mojom::PrintSettingsResult::NewSettings(
      *context->TakeAndResetSettings()));
}

#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
void PrintBackendServiceImpl::AskUserForSettings(
    uint32_t context_id,
    int max_pages,
    bool has_selection,
    bool is_scripted,
    mojom::PrintBackendService::AskUserForSettingsCallback callback) {
  // Safe to use `base::Unretained(this)` because `this` outlives the async
  // call and callback.  The entire service process goes away when `this`
  // lifetime expires.
  PrintingContext* context = GetPrintingContext(context_id);
  CHECK(context) << "No context found for id " << context_id;
  context->AskUserForSettings(
      max_pages, has_selection, is_scripted,
      base::BindOnce(&PrintBackendServiceImpl::OnDidAskUserForSettings,
                     base::Unretained(this), context_id, std::move(callback)));
}
#endif  // BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)

void PrintBackendServiceImpl::UpdatePrintSettings(
    uint32_t context_id,
    base::Value::Dict job_settings,
    mojom::PrintBackendService::UpdatePrintSettingsCallback callback) {
  DCHECK(print_backend_);

  const std::string* printer_name = job_settings.FindString(kSettingDeviceName);
  DCHECK(printer_name);

  crash_keys_ = std::make_unique<crash_keys::ScopedPrinterInfo>(
      *printer_name, print_backend_->GetPrinterDriverInfo(*printer_name));

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_CUPS)
  // Try to fill in advanced settings based upon basic info options.
  PrinterBasicInfo basic_info;
  if (print_backend_->GetPrinterBasicInfo(*printer_name, &basic_info) ==
      mojom::ResultCode::kSuccess) {
    base::Value::Dict advanced_settings;
    for (const auto& pair : basic_info.options)
      advanced_settings.Set(pair.first, pair.second);

    job_settings.Set(kSettingAdvancedSettings, std::move(advanced_settings));
  }
#endif  // BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_CUPS)

  PrintingContext* context = GetPrintingContext(context_id);
  CHECK(context) << "No context found for id " << context_id;
  mojom::ResultCode result =
      context->UpdatePrintSettings(std::move(job_settings));

  if (result != mojom::ResultCode::kSuccess) {
    persistent_printing_contexts_.erase(context_id);
    std::move(callback).Run(mojom::PrintSettingsResult::NewResultCode(result));
    return;
  }

  std::move(callback).Run(
      mojom::PrintSettingsResult::NewSettings(context->settings()));
}

void PrintBackendServiceImpl::StartPrinting(
    uint32_t context_id,
    int document_cookie,
    const std::u16string& document_name,
#if !BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
    const std::optional<PrintSettings>& settings,
#endif
    mojom::PrintBackendService::StartPrintingCallback callback) {
#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CUPS)
  CupsConnectionPool* connection_pool = CupsConnectionPool::GetInstance();
  if (connection_pool) {
    // If a pool exists then this document can only proceed with printing if
    // there is a connection available for use by a `PrintingContext`.
    if (!connection_pool->IsConnectionAvailable()) {
      // This document has to wait until a connection becomes available.  Hold
      // off on issuing the callback.
      // TODO(crbug.com/40561724)  Place this in a queue of waiting jobs.
      DLOG(ERROR) << "Need queue for print jobs awaiting a connection";
      std::move(callback).Run(mojom::ResultCode::kFailed,
                              PrintingContext::kNoPrintJobId);
      return;
    }
  }
#endif

  // This job takes ownership of this printing context and associates it with
  // the document.
  auto item = persistent_printing_contexts_.find(context_id);
  CHECK(item != persistent_printing_contexts_.end())
      << "No context found for id " << context_id;
  std::unique_ptr<ContextContainer> context_container = std::move(item->second);
  persistent_printing_contexts_.erase(item);

#if !BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  if (settings) {
    // Now have the printer name for the print job for use with crash keys.
    std::string printer_name = base::UTF16ToUTF8(settings->device_name());
    crash_keys_ = std::make_unique<crash_keys::ScopedPrinterInfo>(
        printer_name, print_backend_->GetPrinterDriverInfo(printer_name));

    // Apply the settings from the in-browser system dialog to the context.
    context_container->context->SetPrintSettings(*settings);
  }
#endif

  // Save all the document settings for use through the print job, until the
  // time that this document can complete printing.  Track the order of
  // received documents with position in `documents_`.
  auto document = base::MakeRefCounted<PrintedDocument>(
      std::make_unique<PrintSettings>(context_container->context->settings()),
      document_name, document_cookie);
  base::SequenceBound<DocumentContainer> document_container(
      GetPrintingTaskRunner(), std::move(context_container->delegate),
      std::move(context_container->context), document);
  documents_.push_back(std::make_unique<DocumentHelper>(
      document_cookie, std::move(document_container), std::move(callback)));
  DocumentHelper& document_helper = *documents_.back();

  // Safe to use `base::Unretained(this)` because `this` outlives the async
  // call and callback.  The entire service process goes away when `this`
  // lifetime expires.
  document_helper.document_container()
      .AsyncCall(&DocumentContainer::StartPrintingReadyDocument)
      .Then(base::BindOnce(
          &PrintBackendServiceImpl::OnDidStartPrintingReadyDocument,
          base::Unretained(this), std::ref(document_helper)));
}

#if BUILDFLAG(IS_WIN)
void PrintBackendServiceImpl::RenderPrintedPage(
    int32_t document_cookie,
    uint32_t page_index,
    mojom::MetafileDataType page_data_type,
    base::ReadOnlySharedMemoryRegion serialized_page,
    const gfx::Size& page_size,
    const gfx::Rect& page_content_rect,
    float shrink_factor,
    mojom::PrintBackendService::RenderPrintedPageCallback callback) {
  DocumentHelper* document_helper = GetDocumentHelper(document_cookie);
  DCHECK(document_helper);

  document_helper->document_container()
      .AsyncCall(&DocumentContainer::DoRenderPrintedPage)
      .WithArgs(page_index, page_data_type, std::move(serialized_page),
                page_size, page_content_rect, shrink_factor)
      .Then(std::move(callback));
}
#endif  // BUILDFLAG(IS_WIN)

void PrintBackendServiceImpl::RenderPrintedDocument(
    int32_t document_cookie,
    uint32_t page_count,
    mojom::MetafileDataType data_type,
    base::ReadOnlySharedMemoryRegion serialized_document,
    mojom::PrintBackendService::RenderPrintedDocumentCallback callback) {
  DocumentHelper* document_helper = GetDocumentHelper(document_cookie);
  DCHECK(document_helper);

  document_helper->document_container()
      .AsyncCall(&DocumentContainer::DoRenderPrintedDocument)
      .WithArgs(page_count, data_type, std::move(serialized_document))
      .Then(std::move(callback));
}

void PrintBackendServiceImpl::DocumentDone(
    int document_cookie,
    mojom::PrintBackendService::DocumentDoneCallback callback) {
  DocumentHelper* document_helper = GetDocumentHelper(document_cookie);
  DCHECK(document_helper);

  // Safe to use `base::Unretained(this)` because `this` outlives the async
  // call and callback.  The entire service process goes away when `this`
  // lifetime expires.
  document_helper->document_container()
      .AsyncCall(&DocumentContainer::DoDocumentDone)
      .Then(base::BindOnce(&PrintBackendServiceImpl::OnDidDocumentDone,
                           base::Unretained(this), std::ref(*document_helper),
                           std::move(callback)));
}

void PrintBackendServiceImpl::Cancel(
    int document_cookie,
    mojom::PrintBackendService::CancelCallback callback) {
  DCHECK(print_backend_);
  DocumentHelper* document_helper = GetDocumentHelper(document_cookie);
  DCHECK(document_helper);

  // Safe to use `base::Unretained(this)` because `this` outlives the async
  // call and callback.  The entire service process goes away when `this`
  // lifetime expires.
  document_helper->document_container()
      .AsyncCall(&DocumentContainer::DoCancel)
      .Then(base::BindOnce(&PrintBackendServiceImpl::OnDidCancel,
                           base::Unretained(this), std::ref(*document_helper),
                           std::move(callback)));
}

#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
void PrintBackendServiceImpl::OnDidAskUserForSettings(
    uint32_t context_id,
    mojom::PrintBackendService::AskUserForSettingsCallback callback,
    mojom::ResultCode result) {
  auto* context = GetPrintingContext(context_id);
  DCHECK(context);
  if (result != mojom::ResultCode::kSuccess) {
    DLOG(ERROR) << "Did not get user settings, error: " << result;
    persistent_printing_contexts_.erase(context_id);
    std::move(callback).Run(mojom::PrintSettingsResult::NewResultCode(result));
    return;
  }

  // Set the crash keys for the document that should start printing next.
  std::string printer_name =
      base::UTF16ToUTF8(context->settings().device_name());
  crash_keys_ = std::make_unique<crash_keys::ScopedPrinterInfo>(
      printer_name, print_backend_->GetPrinterDriverInfo(printer_name));

  std::move(callback).Run(mojom::PrintSettingsResult::NewSettings(
      *context->TakeAndResetSettings()));
}
#endif  // BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)

void PrintBackendServiceImpl::OnDidStartPrintingReadyDocument(
    DocumentHelper& document_helper,
    StartPrintingResult printing_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  document_helper.TakeStartPrintingCallback().Run(printing_result.result,
                                                  printing_result.job_id);
}

void PrintBackendServiceImpl::OnDidDocumentDone(
    DocumentHelper& document_helper,
    mojom::PrintBackendService::DocumentDoneCallback callback,
    mojom::ResultCode result) {
  std::move(callback).Run(result);

  // The service expects that the calling process will call `Cancel()` if there
  // are any errors during printing.
  if (result == mojom::ResultCode::kSuccess) {
    // All complete for this document.
    RemoveDocumentHelper(document_helper);
  }
}

void PrintBackendServiceImpl::OnDidCancel(
    DocumentHelper& document_helper,
    mojom::PrintBackendService::CancelCallback callback) {
  std::move(callback).Run();

  // Do nothing more with this document.
  RemoveDocumentHelper(document_helper);
}

std::unique_ptr<PrintBackendServiceImpl::PrintingContextDelegate>
PrintBackendServiceImpl::CreatePrintingContextDelegate() {
  auto context_delegate = std::make_unique<PrintingContextDelegate>();
  context_delegate->SetAppLocale(locale_);
  return context_delegate;
}

PrintingContext* PrintBackendServiceImpl::GetPrintingContext(
    uint32_t context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  auto item = persistent_printing_contexts_.find(context_id);
  if (item == persistent_printing_contexts_.end()) {
    return nullptr;
  }
  return item->second->context.get();
}

PrintBackendServiceImpl::DocumentHelper*
PrintBackendServiceImpl::GetDocumentHelper(int document_cookie) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  // Most new calls are expected to be relevant to the most recently added
  // document, which would be at the end of the list.  So search the list
  // backwards to hopefully reduce the time to find the document.
  for (const std::unique_ptr<DocumentHelper>& helper :
       base::Reversed(documents_)) {
    if (helper->document_cookie() == document_cookie) {
      return helper.get();
    }
  }
  return nullptr;
}

void PrintBackendServiceImpl::RemoveDocumentHelper(
    DocumentHelper& document_helper) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  // Must search forwards because std::vector::erase() doesn't work with a
  // reverse iterator.
  int cookie = document_helper.document_cookie();
  auto item =
      base::ranges::find(documents_, cookie, &DocumentHelper::document_cookie);
  CHECK(item != documents_.end(), base::NotFatalUntil::M130)
      << "Document " << cookie << " to be deleted not found";
  documents_.erase(item);

  // TODO(crbug.com/40561724)  This releases a connection; try to start the
  // next job waiting to be started (if any).
}

#if BUILDFLAG(IS_WIN)
base::expected<XpsCapabilities, mojom::ResultCode>
PrintBackendServiceImpl::GetXpsCapabilities(const std::string& printer_name) {
  ASSIGN_OR_RETURN(
      std::string xml,
      print_backend_->GetXmlPrinterCapabilitiesForXpsDriver(printer_name),
      [&](mojom::ResultCode error) {
        DLOG(ERROR) << "Failure getting XPS capabilities of printer "
                    << printer_name << ", error: " << error;
        return error;
      });

  mojom::PrinterCapabilitiesValueResultPtr value_result;
  if (!xml_parser_remote_->ParseXmlForPrinterCapabilities(xml, &value_result)) {
    DLOG(ERROR) << "Failure parsing XML of XPS capabilities of printer "
                << printer_name
                << ", error: ParseXmlForPrinterCapabilities failed";
    return base::unexpected(mojom::ResultCode::kFailed);
  }
  if (value_result->is_result_code()) {
    DLOG(ERROR) << "Failure parsing XML of XPS capabilities of printer "
                << printer_name
                << ", error: " << value_result->get_result_code();
    return base::unexpected(value_result->get_result_code());
  }

  return ParseValueForXpsPrinterCapabilities(value_result->get_capabilities())
      .transform_error([&](mojom::ResultCode code) {
        DLOG(ERROR) << "Failure parsing value of XPS capabilities of printer "
                    << printer_name << ", error: " << code;
        return code;
      });
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace printing
