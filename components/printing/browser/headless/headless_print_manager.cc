// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/headless/headless_print_manager.h"

#include "components/printing/browser/print_to_pdf/pdf_print_result.h"
#include "printing/mojom/print.mojom.h"
#include "printing/printing_utils.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "mojo/public/cpp/bindings/message.h"
#endif

namespace headless {

namespace {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
constexpr char kUnexpectedPrintManagerCall[] =
    "Headless Print Manager: Unexpected Print Manager call";
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
    print_to_pdf::PdfPrintJob::PrintToPdfCallback callback) {
  print_to_pdf::PdfPrintJob::StartJob(
      web_contents(), rfh, GetPrintRenderFrame(rfh), page_ranges,
      std::move(print_pages_params), std::move(callback));
}

void HeadlessPrintManager::GetDefaultPrintSettings(
    GetDefaultPrintSettingsCallback callback) {
  DLOG(ERROR) << "Scripted print is not supported";
  std::move(callback).Run(nullptr);
}

void HeadlessPrintManager::ScriptedPrint(
    printing::mojom::ScriptedPrintParamsPtr params,
    ScriptedPrintCallback callback) {
  DLOG(ERROR) << "Scripted print is not supported";
  std::move(callback).Run(nullptr);
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
void HeadlessPrintManager::UpdatePrintSettings(
    base::Value::Dict job_settings,
    UpdatePrintSettingsCallback callback) {
  mojo::ReportBadMessage(kUnexpectedPrintManagerCall);
}

void HeadlessPrintManager::SetupScriptedPrintPreview(
    SetupScriptedPrintPreviewCallback callback) {
  DLOG(ERROR) << "Scripted print preview is not supported";
  std::move(callback).Run();
}

void HeadlessPrintManager::ShowScriptedPrintPreview(bool source_is_modifiable) {
  DLOG(ERROR) << "Scripted print preview is not supported";
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

void HeadlessPrintManager::SetAccessibilityTree(
    int32_t cookie,
    const ui::AXTreeUpdate& accessibility_tree) {
  mojo::ReportBadMessage(kUnexpectedPrintManagerCall);
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

#if BUILDFLAG(IS_ANDROID)
void HeadlessPrintManager::PdfWritingDone(int page_count) {}
#endif

WEB_CONTENTS_USER_DATA_KEY_IMPL(HeadlessPrintManager);

}  // namespace headless
