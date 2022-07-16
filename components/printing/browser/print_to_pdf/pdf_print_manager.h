// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_BROWSER_PRINT_TO_PDF_PDF_PRINT_MANAGER_H_
#define COMPONENTS_PRINTING_BROWSER_PRINT_TO_PDF_PDF_PRINT_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "build/build_config.h"
#include "components/printing/browser/print_manager.h"
#include "components/printing/browser/print_to_pdf/pdf_print_result.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "printing/print_settings.h"

namespace print_to_pdf {

class PdfPrintManager : public printing::PrintManager,
                        public content::WebContentsUserData<PdfPrintManager> {
 public:
  using PrintToPdfCallback =
      base::OnceCallback<void(PdfPrintResult,
                              scoped_refptr<base::RefCountedMemory>)>;

  ~PdfPrintManager() override;

  PdfPrintManager(const PdfPrintManager&) = delete;
  PdfPrintManager& operator=(const PdfPrintManager&) = delete;

  static void BindPrintManagerHost(
      mojo::PendingAssociatedReceiver<printing::mojom::PrintManagerHost>
          receiver,
      content::RenderFrameHost* rfh);

  void PrintToPdf(content::RenderFrameHost* rfh,
                  const std::string& page_ranges,
                  printing::mojom::PrintPagesParamsPtr print_page_params,
                  PrintToPdfCallback callback);

 private:
  friend class content::WebContentsUserData<PdfPrintManager>;

  explicit PdfPrintManager(content::WebContents* web_contents);

  void OnDidPrintWithParams(printing::mojom::PrintWithParamsResultPtr result);

  // WebContentsObserver overrides (via PrintManager):
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  // printing::mojom::PrintManagerHost:
  void GetDefaultPrintSettings(
      GetDefaultPrintSettingsCallback callback) override;
  void ScriptedPrint(printing::mojom::ScriptedPrintParamsPtr params,
                     ScriptedPrintCallback callback) override;
  void ShowInvalidPrinterSettingsError() override;
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  void UpdatePrintSettings(int32_t cookie,
                           base::Value::Dict job_settings,
                           UpdatePrintSettingsCallback callback) override;
  void SetupScriptedPrintPreview(
      SetupScriptedPrintPreviewCallback callback) override;
  void ShowScriptedPrintPreview(bool source_is_modifiable) override;
  void RequestPrintPreview(
      printing::mojom::RequestPrintPreviewParamsPtr params) override;
  void CheckForCancel(int32_t preview_ui_id,
                      int32_t request_id,
                      CheckForCancelCallback callback) override;
#endif
#if BUILDFLAG(ENABLE_TAGGED_PDF)
  void SetAccessibilityTree(
      int32_t cookie,
      const ui::AXTreeUpdate& accessibility_tree) override;
#endif
#if BUILDFLAG(IS_ANDROID)
  void PdfWritingDone(int page_count) override;
#endif

  void Reset();
  void ReleaseJob(PdfPrintResult result);

  raw_ptr<content::RenderFrameHost> printing_rfh_ = nullptr;
  PrintToPdfCallback callback_;
  std::string data_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace print_to_pdf

#endif  // COMPONENTS_PRINTING_BROWSER_PRINT_TO_PDF_PDF_PRINT_MANAGER_H_
