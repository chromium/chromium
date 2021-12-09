// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/printing/print_backend_service_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "components/crash/core/common/crash_keys.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "printing/backend/print_backend.h"
#include "printing/mojom/print.mojom.h"
#include "printing/printing_context.h"

#if defined(OS_MAC)
#include "base/threading/thread_restrictions.h"
#include "chrome/common/printing/printer_capabilities_mac.h"
#endif

#if defined(OS_CHROMEOS) && defined(USE_CUPS)
#include "printing/backend/cups_connection_pool.h"
#endif

namespace printing {

namespace {

scoped_refptr<base::SequencedTaskRunner> GetPrintingTaskRunner() {
  static constexpr base::TaskTraits kTraits = {
      base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

#if defined(USE_CUPS)
  // CUPS is thread safe, so a task runner can be allocated for each job.
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(kTraits);
#elif defined(OS_WIN)
  // For Windows, we want a single threaded task runner shared for all print
  // jobs in the process because Windows printer drivers are oftentimes not
  // thread-safe.  This protects against multiple print jobs to the same device
  // from running in the driver at the same time.
  static scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSingleThreadTaskRunner(kTraits);
#else
  // Be conservative for unsupported platforms, use a single threaded runner
  // so that concurrent print jobs are not in driver code at the same time.
  static scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSingleThreadTaskRunner(kTraits);
#endif

  return task_runner;
}

// Local storage of document and associated data needed to submit to job to
// the operating system's printing API.  All access to the document occurs on
// a worker task runner.
class DocumentContainer {
 public:
  DocumentContainer(PrintingContext::Delegate* context_delegate,
                    scoped_refptr<PrintedDocument> document,
                    mojom::PrintTargetType target_type)
      : context_delegate_(context_delegate),
        document_(document),
        target_type_(target_type) {}

  ~DocumentContainer() = default;

  // Helper function that runs on a task runner.
  mojom::ResultCode StartPrintingReadyDocument();

 private:
  PrintingContext::Delegate* context_delegate_;
  scoped_refptr<PrintedDocument> document_;

  // `context` is not initialized until the document is ready for printing.
  std::unique_ptr<PrintingContext> context_;

  // Parameter required for the delayed call to `UpdatePrinterSettings()`.
  mojom::PrintTargetType target_type_;

  // Ensure all interactions for this document are issued from the same runner.
  SEQUENCE_CHECKER(sequence_checker_);
};

mojom::ResultCode DocumentContainer::StartPrintingReadyDocument() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOG(1) << "Start printing for document " << document_->cookie();

  // Create a printing context that will work with this document for the
  // duration of the print job.
  context_ =
      PrintingContext::Create(context_delegate_, /*skip_system_calls=*/false);

  // With out-of-process printing the printer settings no longer get updated
  // from `PrintingContext::UpdatePrintSettings()`, so we need to apply that
  // now to our new context.
  // TODO(crbug.com/1245679)  Replumb `mojom::PrintTargetType` into
  // `PrintingContext::UpdatePrinterSettings()`.
  PrintingContext::PrinterSettings printer_settings {
#if defined(OS_MAC)
    .external_preview =
        target_type_ == mojom::PrintTargetType::kExternalPreview,
#endif
    .show_system_dialog = target_type_ == mojom::PrintTargetType::kSystemDialog,
#if defined(OS_WIN)
    .page_count = 0,
#endif
  };
  context_->ApplyPrintSettings(document_->settings());
  mojom::ResultCode result = context_->UpdatePrinterSettings(printer_settings);
  if (result != mojom::ResultCode::kSuccess) {
    DLOG(ERROR) << "Failure updating printer settings for document "
                << document_->cookie() << ", error: " << result;
    return result;
  }

  result = context_->NewDocument(document_->name());
  if (result != mojom::ResultCode::kSuccess) {
    DLOG(ERROR) << "Failure initializing new document " << document_->cookie()
                << ", error: " << result;
    return result;
  }

  return mojom::ResultCode::kSuccess;
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
  NOTREACHED();
  return nullptr;
}

std::string PrintBackendServiceImpl::PrintingContextDelegate::GetAppLocale() {
  return locale_;
}

void PrintBackendServiceImpl::PrintingContextDelegate::SetAppLocale(
    const std::string& locale) {
  locale_ = locale;
}

PrintBackendServiceImpl::PrintBackendServiceImpl(
    mojo::PendingReceiver<mojom::PrintBackendService> receiver)
    : receiver_(this, std::move(receiver)) {}

PrintBackendServiceImpl::~PrintBackendServiceImpl() = default;

void PrintBackendServiceImpl::Init(const std::string& locale) {
  print_backend_ = PrintBackend::CreateInstance(locale);
  context_delegate_.SetAppLocale(locale);
}

// TODO(crbug.com/1225111)  Do nothing, this is just to assist an idle timeout
// change by providing a low-cost call to ensure it is applied.
void PrintBackendServiceImpl::Poke() {}

void PrintBackendServiceImpl::EnumeratePrinters(
    mojom::PrintBackendService::EnumeratePrintersCallback callback) {
  if (!print_backend_) {
    DLOG(ERROR)
        << "Print backend instance has not been initialized for locale.";
    std::move(callback).Run(
        mojom::PrinterListResult::NewResultCode(mojom::ResultCode::kFailed));
    return;
  }

  PrinterList printer_list;
  mojom::ResultCode result = print_backend_->EnumeratePrinters(&printer_list);
  if (result != mojom::ResultCode::kSuccess) {
    std::move(callback).Run(mojom::PrinterListResult::NewResultCode(result));
    return;
  }
  std::move(callback).Run(
      mojom::PrinterListResult::NewPrinterList(std::move(printer_list)));
}

void PrintBackendServiceImpl::GetDefaultPrinterName(
    mojom::PrintBackendService::GetDefaultPrinterNameCallback callback) {
  if (!print_backend_) {
    DLOG(ERROR)
        << "Print backend instance has not been initialized for locale.";
    std::move(callback).Run(mojom::DefaultPrinterNameResult::NewResultCode(
        mojom::ResultCode::kFailed));
    return;
  }
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

void PrintBackendServiceImpl::GetPrinterSemanticCapsAndDefaults(
    const std::string& printer_name,
    mojom::PrintBackendService::GetPrinterSemanticCapsAndDefaultsCallback
        callback) {
  if (!print_backend_) {
    DLOG(ERROR)
        << "Print backend instance has not been initialized for locale.";
    std::move(callback).Run(
        mojom::PrinterSemanticCapsAndDefaultsResult::NewResultCode(
            mojom::ResultCode::kFailed));
    return;
  }

  crash_keys_ = std::make_unique<crash_keys::ScopedPrinterInfo>(
      print_backend_->GetPrinterDriverInfo(printer_name));

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

void PrintBackendServiceImpl::FetchCapabilities(
    const std::string& printer_name,
    mojom::PrintBackendService::FetchCapabilitiesCallback callback) {
  if (!print_backend_) {
    DLOG(ERROR)
        << "Print backend instance has not been initialized for locale.";
    std::move(callback).Run(mojom::PrinterCapsAndInfoResult::NewResultCode(
        mojom::ResultCode::kFailed));
    return;
  }

  crash_keys_ = std::make_unique<crash_keys::ScopedPrinterInfo>(
      print_backend_->GetPrinterDriverInfo(printer_name));

  PrinterSemanticCapsAndDefaults::Papers user_defined_papers;
#if defined(OS_MAC)
  {
    // Blocking is needed here for when macOS reads paper sizes from file.
    //
    // Fetching capabilities in the browser process happens from the thread
    // pool with the MayBlock() trait for macOS.  However this call can also
    // run from a utility process's main thread where blocking is not
    // implicitly allowed.  In order to preserve ordering, the utility process
    // must process this synchronously by blocking.
    //
    // TODO(crbug.com/1163635):  Investigate whether utility process main
    // thread should be allowed to block like in-process workers are.
    base::ScopedAllowBlocking allow_blocking;
    user_defined_papers = GetMacCustomPaperSizes();
  }
#endif

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
  mojom::PrinterCapsAndInfoPtr caps_and_info = mojom::PrinterCapsAndInfo::New(
      std::move(printer_info), std::move(user_defined_papers), std::move(caps));
  std::move(callback).Run(
      mojom::PrinterCapsAndInfoResult::NewPrinterCapsAndInfo(
          std::move(caps_and_info)));
}

void PrintBackendServiceImpl::UpdatePrintSettings(
    base::flat_map<std::string, base::Value> job_settings,
    mojom::PrintBackendService::UpdatePrintSettingsCallback callback) {
  if (!print_backend_) {
    DLOG(ERROR)
        << "Print backend instance has not been initialized for locale.";
    std::move(callback).Run(
        mojom::PrintSettingsResult::NewResultCode(mojom::ResultCode::kFailed));
    return;
  }

  auto item = job_settings.find(kSettingDeviceName);
  if (item == job_settings.end()) {
    DLOG(ERROR) << "Job settings are missing specification of printer name";
    std::move(callback).Run(
        mojom::PrintSettingsResult::NewResultCode(mojom::ResultCode::kFailed));
    return;
  }
  const base::Value& device_name_value = item->second;
  if (!device_name_value.is_string()) {
    DLOG(ERROR) << "Invalid type for job settings device name entry, is type "
                << device_name_value.type();
    std::move(callback).Run(
        mojom::PrintSettingsResult::NewResultCode(mojom::ResultCode::kFailed));
    return;
  }
  const std::string& printer_name = device_name_value.GetString();

  crash_keys_ = std::make_unique<crash_keys::ScopedPrinterInfo>(
      print_backend_->GetPrinterDriverInfo(printer_name));

#if defined(OS_LINUX) && defined(USE_CUPS)
  // Try to fill in advanced settings based upon basic info options.
  PrinterBasicInfo basic_info;
  if (print_backend_->GetPrinterBasicInfo(printer_name, &basic_info) ==
      mojom::ResultCode::kSuccess) {
    base::Value advanced_settings(base::Value::Type::DICTIONARY);
    for (const auto& pair : basic_info.options)
      advanced_settings.SetStringKey(pair.first, pair.second);

    job_settings[kSettingAdvancedSettings] = std::move(advanced_settings);
  }
#endif  // defined(OS_LINUX) && defined(USE_CUPS)

  // Use a one-time `PrintingContext` to do the update to print settings.
  // Intentionally do not cache this context here since the process model does
  // not guarantee that we will return to this same process when
  // `StartPrinting()` might be called.
  std::unique_ptr<PrintingContext> context =
      PrintingContext::Create(&context_delegate_, /*skip_system_calls=*/false);
  mojom::ResultCode result =
      context->UpdatePrintSettings(base::Value(std::move(job_settings)));

  if (result != mojom::ResultCode::kSuccess) {
    std::move(callback).Run(mojom::PrintSettingsResult::NewResultCode(result));
    return;
  }

  std::move(callback).Run(mojom::PrintSettingsResult::NewSettings(
      *context->TakeAndResetSettings()));
}

void PrintBackendServiceImpl::StartPrinting(
    int document_cookie,
    const std::u16string& document_name,
    mojom::PrintTargetType target_type,
    const PrintSettings& settings,
    mojom::PrintBackendService::StartPrintingCallback callback) {
  if (!print_backend_) {
    DLOG(ERROR)
        << "Print backend instance has not been initialized for locale.";
    std::move(callback).Run(mojom::ResultCode::kFailed);
    return;
  }

#if defined(OS_CHROMEOS) && defined(USE_CUPS)
  CupsConnectionPool* connection_pool = CupsConnectionPool::GetInstance();
  if (connection_pool) {
    // If a pool exists then this document can only proceed with printing if
    // there is a connection available for use by a `PrintingContext`.
    if (!connection_pool->IsConnectionAvailable()) {
      // This document has to wait until a connection becomes available.  Hold
      // off on issuing the callback.
      // TODO(crbug.com/809738)  Place this in a queue of waiting jobs.
      DLOG(ERROR) << "Need queue for print jobs awaiting a connection";
      std::move(callback).Run(mojom::ResultCode::kFailed);
      return;
    }
  }
#endif

  // Save all the document settings for use through the print job, until the
  // time that this document can complete printing.  Track the order of
  // received documents with position in `documents_`.
  auto document = base::MakeRefCounted<PrintedDocument>(
      std::make_unique<PrintSettings>(settings), document_name,
      document_cookie);
  base::SequenceBound<DocumentContainer> document_container(
      GetPrintingTaskRunner(), &context_delegate_, document, target_type);
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

void PrintBackendServiceImpl::OnDidStartPrintingReadyDocument(
    DocumentHelper& document_helper,
    mojom::ResultCode result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(callback_sequence_checker_);
  document_helper.TakeStartPrintingCallback().Run(result);
  if (result == mojom::ResultCode::kSuccess)
    return;

  // Remove this document due to the failure to do setup.
  int cookie = document_helper.document_cookie();
  auto item =
      std::find_if(documents_.begin(), documents_.end(),
                   [cookie](const std::unique_ptr<DocumentHelper>& helper) {
                     return helper->document_cookie() == cookie;
                   });
  DCHECK(item != documents_.end())
      << "Document " << cookie << " to be deleted not found";
  documents_.erase(item);

  // TODO(crbug.com/809738)  This releases a connection; try to start the
  // next job waiting to be started (if any).
}

}  // namespace printing
