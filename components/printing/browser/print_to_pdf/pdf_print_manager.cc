// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/print_to_pdf/pdf_print_manager.h"

#include <utility>

#include "build/build_config.h"
#include "components/printing/browser/print_to_pdf/pdf_print_utils.h"
#include "printing/mojom/print.mojom.h"
#include "printing/page_range.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "mojo/public/cpp/bindings/message.h"
#endif

namespace print_to_pdf {

namespace {

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
constexpr char kInvalidUpdatePrintSettingsCall[] =
    "Invalid UpdatePrintSettings Call";
constexpr char kInvalidSetupScriptedPrintPreviewCall[] =
    "Invalid SetupScriptedPrintPreview Call";
constexpr char kInvalidShowScriptedPrintPreviewCall[] =
    "Invalid ShowScriptedPrintPreview Call";
constexpr char kInvalidRequestPrintPreviewCall[] =
    "Invalid RequestPrintPreview Call";
constexpr char kInvalidCheckForCancelCall[] = "Invalid CheckForCancel Call";
#endif

#if BUILDFLAG(ENABLE_TAGGED_PDF)
constexpr char kInvalidSetAccessibilityTreeCall[] =
    "Invalid SetAccessibilityTree Call";
#endif

}  // namespace

PdfPrintManager::PdfPrintManager(content::WebContents* web_contents)
    : printing::PrintManager(web_contents),
      content::WebContentsUserData<PdfPrintManager>(*web_contents) {}

PdfPrintManager::~PdfPrintManager() = default;

// static
void PdfPrintManager::BindPrintManagerHost(
    mojo::PendingAssociatedReceiver<printing::mojom::PrintManagerHost> receiver,
    content::RenderFrameHost* rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;

  auto* print_manager = PdfPrintManager::FromWebContents(web_contents);
  if (!print_manager)
    return;

  print_manager->BindReceiver(std::move(receiver), rfh);
}

// static
std::string PdfPrintManager::PrintResultToString(PrintResult result) {
  switch (result) {
    case PRINT_SUCCESS:
      return std::string();  // no error message
    case PRINTING_FAILED:
      return "Printing failed";
    case INVALID_PRINTER_SETTINGS:
      return "Show invalid printer settings error";
    case INVALID_MEMORY_HANDLE:
      return "Invalid memory handle";
    case METAFILE_MAP_ERROR:
      return "Map to shared memory error";
    case METAFILE_INVALID_HEADER:
      return "Invalid metafile header";
    case METAFILE_GET_DATA_ERROR:
      return "Get data from metafile error";
    case SIMULTANEOUS_PRINT_ACTIVE:
      return "The previous printing job hasn't finished";
    case PAGE_RANGE_SYNTAX_ERROR:
      return "Page range syntax error";
    case PAGE_COUNT_EXCEEDED:
      return "Page range exceeds page count";
    default:
      NOTREACHED();
      return "Unknown PrintResult";
  }
}

void PdfPrintManager::PrintToPdf(
    content::RenderFrameHost* rfh,
    const std::string& page_ranges,
    bool ignore_invalid_page_ranges,
    printing::mojom::PrintPagesParamsPtr print_pages_params,
    PrintToPdfCallback callback) {
  DCHECK(callback);

  if (callback_) {
    std::move(callback).Run(SIMULTANEOUS_PRINT_ACTIVE,
                            base::MakeRefCounted<base::RefCountedString>());
    return;
  }

  if (!rfh->IsRenderFrameLive()) {
    std::move(callback).Run(PRINTING_FAILED,
                            base::MakeRefCounted<base::RefCountedString>());
    return;
  }

  printing_rfh_ = rfh;
  page_ranges_ = page_ranges;
  ignore_invalid_page_ranges_ = ignore_invalid_page_ranges;
  print_pages_params_ = std::move(print_pages_params);
  set_cookie(print_pages_params_->params->document_cookie);
  callback_ = std::move(callback);

  GetPrintRenderFrame(rfh)->PrintRequestedPages();
}

void PdfPrintManager::GetDefaultPrintSettings(
    GetDefaultPrintSettingsCallback callback) {
  if (!printing_rfh_) {
    DLOG(ERROR) << "Unexpected message received before PrintToPdf is "
                   "called: GetDefaultPrintSettings";
    std::move(callback).Run(printing::mojom::PrintParams::New());
    return;
  }
  std::move(callback).Run(print_pages_params_->params->Clone());
}

void PdfPrintManager::ScriptedPrint(
    printing::mojom::ScriptedPrintParamsPtr params,
    ScriptedPrintCallback callback) {
  auto default_param = printing::mojom::PrintPagesParams::New();
  default_param->params = printing::mojom::PrintParams::New();
  if (!printing_rfh_) {
    DLOG(ERROR) << "Unexpected message received before PrintToPdf is "
                   "called: ScriptedPrint";
    std::move(callback).Run(std::move(default_param));
    return;
  }
  if (params->is_scripted &&
      GetCurrentTargetFrame()->IsNestedWithinFencedFrame()) {
    DLOG(ERROR) << "Unexpected message received. Script Print is not allowed"
                   " in a fenced frame.";
    std::move(callback).Run(std::move(default_param));
    return;
  }
  absl::variant<printing::PageRanges, PageRangeError> page_ranges =
      TextPageRangesToPageRanges(page_ranges_, ignore_invalid_page_ranges_,
                                 params->expected_pages_count);
  if (absl::holds_alternative<PageRangeError>(page_ranges)) {
    PrintResult print_result;
    switch (absl::get<PageRangeError>(page_ranges)) {
      case PageRangeError::SYNTAX_ERROR:
        print_result = PAGE_RANGE_SYNTAX_ERROR;
        break;
      case PageRangeError::LIMIT_ERROR:
        print_result = PAGE_COUNT_EXCEEDED;
        break;
    }
    ReleaseJob(print_result);
    std::move(callback).Run(std::move(default_param));
    return;
  }

  DCHECK(absl::holds_alternative<printing::PageRanges>(page_ranges));
  print_pages_params_->pages = printing::PageRange::GetPages(
      absl::get<printing::PageRanges>(page_ranges));

  std::move(callback).Run(print_pages_params_->Clone());
}

void PdfPrintManager::ShowInvalidPrinterSettingsError() {
  ReleaseJob(INVALID_PRINTER_SETTINGS);
}

void PdfPrintManager::PrintingFailed(int32_t cookie) {
  ReleaseJob(PRINTING_FAILED);
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
void PdfPrintManager::UpdatePrintSettings(
    int32_t cookie,
    base::Value::Dict job_settings,
    UpdatePrintSettingsCallback callback) {
  // UpdatePrintSettingsCallback() should never be called on
  // PdfPrintManager, since it is only triggered by Print Preview.
  mojo::ReportBadMessage(kInvalidUpdatePrintSettingsCall);
}

void PdfPrintManager::SetupScriptedPrintPreview(
    SetupScriptedPrintPreviewCallback callback) {
  // SetupScriptedPrintPreview() should never be called on
  // PdfPrintManager, since it is only triggered by Print Preview.
  mojo::ReportBadMessage(kInvalidSetupScriptedPrintPreviewCall);
}

void PdfPrintManager::ShowScriptedPrintPreview(bool source_is_modifiable) {
  // ShowScriptedPrintPreview() should never be called on
  // PdfPrintManager, since it is only triggered by Print Preview.
  mojo::ReportBadMessage(kInvalidShowScriptedPrintPreviewCall);
}

void PdfPrintManager::RequestPrintPreview(
    printing::mojom::RequestPrintPreviewParamsPtr params) {
  // RequestPrintPreview() should never be called on PdfPrintManager,
  // since it is only triggered by Print Preview.
  mojo::ReportBadMessage(kInvalidRequestPrintPreviewCall);
}

void PdfPrintManager::CheckForCancel(int32_t preview_ui_id,
                                     int32_t request_id,
                                     CheckForCancelCallback callback) {
  // CheckForCancel() should never be called on PdfPrintManager, since it
  // is only triggered by Print Preview.
  mojo::ReportBadMessage(kInvalidCheckForCancelCall);
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

#if BUILDFLAG(ENABLE_TAGGED_PDF)
void PdfPrintManager::SetAccessibilityTree(
    int32_t cookie,
    const ui::AXTreeUpdate& accessibility_tree) {
  // SetAccessibilityTree() should never be called on PdfPrintManager,
  // since it is only triggered by Print Preview.
  mojo::ReportBadMessage(kInvalidSetAccessibilityTreeCall);
}
#endif

#if BUILDFLAG(IS_ANDROID)
void PdfPrintManager::PdfWritingDone(int page_count) {}
#endif

void PdfPrintManager::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  PrintManager::RenderFrameDeleted(render_frame_host);

  if (printing_rfh_ != render_frame_host) {
    return;
  }

  if (callback_) {
    std::move(callback_).Run(PRINTING_FAILED,
                             base::MakeRefCounted<base::RefCountedString>());
  }

  Reset();
}

void PdfPrintManager::DidPrintDocument(
    printing::mojom::DidPrintDocumentParamsPtr params,
    DidPrintDocumentCallback callback) {
  auto& content = *params->content;
  if (!content.metafile_data_region.IsValid()) {
    ReleaseJob(INVALID_MEMORY_HANDLE);
    std::move(callback).Run(false);
    return;
  }
  base::ReadOnlySharedMemoryMapping map = content.metafile_data_region.Map();
  if (!map.IsValid()) {
    ReleaseJob(METAFILE_MAP_ERROR);
    std::move(callback).Run(false);
    return;
  }
  data_ = std::string(static_cast<const char*>(map.memory()), map.size());
  std::move(callback).Run(true);
  ReleaseJob(PRINT_SUCCESS);
}

void PdfPrintManager::Reset() {
  printing_rfh_ = nullptr;
  callback_.Reset();
  print_pages_params_.reset();
  data_.clear();
}

void PdfPrintManager::ReleaseJob(PrintResult result) {
  if (!callback_) {
    DLOG(ERROR) << "ReleaseJob is called when callback_ is null. Check whether "
                   "ReleaseJob is called more than once.";
    return;
  }

  DCHECK(result == PRINT_SUCCESS || data_.empty());
  std::move(callback_).Run(result, base::RefCountedString::TakeString(&data_));
  // TODO(https://crbug.com/1286556): In theory, this should not be needed. In
  // practice, nothing seems to restrict receiving incoming Mojo method calls
  // for reporting the printing state to `printing_rfh_`.
  //
  // This should probably be changed so that the browser pushes endpoints to the
  // renderer rather than the renderer connecting on-demand to the browser...
  if (printing_rfh_ && printing_rfh_->IsRenderFrameLive()) {
    GetPrintRenderFrame(printing_rfh_)->PrintingDone(result == PRINT_SUCCESS);
  }
  Reset();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PdfPrintManager);

}  // namespace print_to_pdf
