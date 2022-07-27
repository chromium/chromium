// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/headless/headless_print_manager.h"

#include <utility>

#include "base/bind.h"
#include "build/build_config.h"
#include "components/printing/browser/print_to_pdf/pdf_print_utils.h"
#include "printing/mojom/print.mojom.h"
#include "printing/page_range.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "mojo/public/cpp/bindings/message.h"
#endif

using print_to_pdf::PdfPrintResult;

namespace headless {

namespace {

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
constexpr char kUnexpectedPrintManagerCall[] = "Unexpected Print Manager call";
#endif

}  // namespace

HeadlessPrintManager::HeadlessPrintManager(content::WebContents* web_contents)
    : printing::PrintManager(web_contents),
      content::WebContentsUserData<HeadlessPrintManager>(*web_contents) {}

HeadlessPrintManager::~HeadlessPrintManager() = default;

// static
void HeadlessPrintManager::BindPrintManagerHost(
    mojo::PendingAssociatedReceiver<printing::mojom::PrintManagerHost> receiver,
    content::RenderFrameHost* rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;

  auto* print_manager = HeadlessPrintManager::FromWebContents(web_contents);
  if (!print_manager)
    return;

  print_manager->BindReceiver(std::move(receiver), rfh);
}

void HeadlessPrintManager::PrintToPdf(
    content::RenderFrameHost* rfh,
    const std::string& page_ranges,
    printing::mojom::PrintPagesParamsPtr print_pages_params,
    PrintToPdfCallback callback) {
  DCHECK(callback);

  if (print_to_pdf_callback_) {
    std::move(callback).Run(PdfPrintResult::kSimultaneousPrintActive,
                            base::MakeRefCounted<base::RefCountedString>());
    return;
  }

  if (!rfh->IsRenderFrameLive()) {
    std::move(callback).Run(PdfPrintResult::kPrintFailure,
                            base::MakeRefCounted<base::RefCountedString>());
    return;
  }

  absl::variant<printing::PageRanges, PdfPrintResult> parsed_ranges =
      print_to_pdf::TextPageRangesToPageRanges(page_ranges);
  if (absl::holds_alternative<PdfPrintResult>(parsed_ranges)) {
    DCHECK_NE(absl::get<PdfPrintResult>(parsed_ranges),
              PdfPrintResult::kPrintSuccess);
    std::move(callback).Run(absl::get<PdfPrintResult>(parsed_ranges),
                            base::MakeRefCounted<base::RefCountedString>());
    return;
  }

  printing_rfh_ = rfh;
  print_pages_params->pages = absl::get<printing::PageRanges>(parsed_ranges);
  print_to_pdf_callback_ = std::move(callback);

  // There is no need for a weak pointer here since the mojo proxy is held
  // in the base class. If we're gone, mojo will discard the callback.
  GetPrintRenderFrame(rfh)->PrintWithParams(
      std::move(print_pages_params),
      base::BindOnce(&HeadlessPrintManager::OnDidPrintWithParams,
                     base::Unretained(this)));
}

void HeadlessPrintManager::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  PrintManager::RenderFrameDeleted(render_frame_host);

  if (printing_rfh_ != render_frame_host)
    return;

  FailJob(PdfPrintResult::kPrintFailure);
}

void HeadlessPrintManager::GetDefaultPrintSettings(
    GetDefaultPrintSettingsCallback callback) {
  DLOG(ERROR) << "Scripted print is not supported";
  std::move(callback).Run(printing::mojom::PrintParams::New());
}

void HeadlessPrintManager::ScriptedPrint(
    printing::mojom::ScriptedPrintParamsPtr params,
    ScriptedPrintCallback callback) {
  auto default_param = printing::mojom::PrintPagesParams::New();
  default_param->params = printing::mojom::PrintParams::New();
  DLOG(ERROR) << "Scripted print is not supported";
  std::move(callback).Run(std::move(default_param));
}

void HeadlessPrintManager::ShowInvalidPrinterSettingsError() {
  FailJob(PdfPrintResult::kInvalidPrinterSettings);
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
void HeadlessPrintManager::UpdatePrintSettings(
    int32_t cookie,
    base::Value::Dict job_settings,
    UpdatePrintSettingsCallback callback) {
  mojo::ReportBadMessage(kUnexpectedPrintManagerCall);
}

void HeadlessPrintManager::SetupScriptedPrintPreview(
    SetupScriptedPrintPreviewCallback callback) {
  mojo::ReportBadMessage(kUnexpectedPrintManagerCall);
}

void HeadlessPrintManager::ShowScriptedPrintPreview(bool source_is_modifiable) {
  mojo::ReportBadMessage(kUnexpectedPrintManagerCall);
}

void HeadlessPrintManager::RequestPrintPreview(
    printing::mojom::RequestPrintPreviewParamsPtr params) {
  mojo::ReportBadMessage(kUnexpectedPrintManagerCall);
}

void HeadlessPrintManager::CheckForCancel(int32_t preview_ui_id,
                                          int32_t request_id,
                                          CheckForCancelCallback callback) {
  mojo::ReportBadMessage(kUnexpectedPrintManagerCall);
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

#if BUILDFLAG(ENABLE_TAGGED_PDF)
void HeadlessPrintManager::SetAccessibilityTree(
    int32_t cookie,
    const ui::AXTreeUpdate& accessibility_tree) {
  mojo::ReportBadMessage(kUnexpectedPrintManagerCall);
}
#endif

#if BUILDFLAG(IS_ANDROID)
void HeadlessPrintManager::PdfWritingDone(int page_count) {}
#endif

void HeadlessPrintManager::OnDidPrintWithParams(
    printing::mojom::PrintWithParamsResultPtr result) {
  if (result->is_failure_reason()) {
    switch (result->get_failure_reason()) {
      case printing::mojom::PrintFailureReason::kGeneralFailure:
        FailJob(PdfPrintResult::kPrintFailure);
        return;
      case printing::mojom::PrintFailureReason::kInvalidPageRange:
        FailJob(PdfPrintResult::kPageCountExceeded);
        return;
    }
  }

  auto& content = *result->get_params()->content;
  if (!content.metafile_data_region.IsValid()) {
    FailJob(PdfPrintResult::kInvalidMemoryHandle);
    return;
  }

  base::ReadOnlySharedMemoryMapping map = content.metafile_data_region.Map();
  if (!map.IsValid()) {
    FailJob(PdfPrintResult::kMetafileMapError);
    return;
  }

  std::string data =
      std::string(static_cast<const char*>(map.memory()), map.size());
  std::move(print_to_pdf_callback_)
      .Run(PdfPrintResult::kPrintSuccess,
           base::RefCountedString::TakeString(&data));

  Reset();
}

void HeadlessPrintManager::FailJob(PdfPrintResult result) {
  DCHECK_NE(result, PdfPrintResult::kPrintSuccess);

  if (print_to_pdf_callback_) {
    std::move(print_to_pdf_callback_)
        .Run(result, base::MakeRefCounted<base::RefCountedString>());
  }

  Reset();
}

void HeadlessPrintManager::Reset() {
  printing_rfh_ = nullptr;

  // The callback is supposed to be consumed at this point meaning we
  // reported results to the PrintToPdf() caller.
  CHECK(!print_to_pdf_callback_);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(HeadlessPrintManager);

}  // namespace headless
