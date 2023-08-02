// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_BROWSER_HEADLESS_HEADLESS_PRINT_MANAGER_H_
#define COMPONENTS_PRINTING_BROWSER_HEADLESS_HEADLESS_PRINT_MANAGER_H_

#include <memory>
#include <string>

#include "build/build_config.h"
#include "components/printing/browser/print_manager.h"
#include "components/printing/browser/print_to_pdf/pdf_print_job.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "printing/print_settings.h"

namespace headless {

// Minimalistic PrintManager implemementation intended for use with Headless
// Chrome. It shortcuts most of the methods exposing only PrintToPdf()
// functionality.
class HeadlessPrintManager
    : public printing::PrintManager,
      public content::WebContentsUserData<HeadlessPrintManager> {
 public:
  ~HeadlessPrintManager() override;

  HeadlessPrintManager(const HeadlessPrintManager&) = delete;
  HeadlessPrintManager& operator=(const HeadlessPrintManager&) = delete;

  static void BindPrintManagerHost(
      mojo::PendingAssociatedReceiver<printing::mojom::PrintManagerHost>
          receiver,
      content::RenderFrameHost* rfh);

  void PrintToPdf(content::RenderFrameHost* rfh,
                  const std::string& page_ranges,
                  printing::mojom::PrintPagesParamsPtr print_page_params,
                  print_to_pdf::PdfPrintJob::PrintToPdfCallback callback);

 private:
  friend class content::WebContentsUserData<HeadlessPrintManager>;

  explicit HeadlessPrintManager(content::WebContents* web_contents);

  // printing::mojom::PrintManagerHost:
  void GetDefaultPrintSettings(
      GetDefaultPrintSettingsCallback callback) override;
  void ScriptedPrint(printing::mojom::ScriptedPrintParamsPtr params,
                     ScriptedPrintCallback callback) override;
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  void UpdatePrintSettings(base::Value::Dict job_settings,
                           UpdatePrintSettingsCallback callback) override;
  void SetupScriptedPrintPreview(
      SetupScriptedPrintPreviewCallback callback) override;
  void ShowScriptedPrintPreview(bool source_is_modifiable) override;
  void RequestPrintPreview(
      printing::mojom::RequestPrintPreviewParamsPtr params) override;
  void CheckForCancel(int32_t preview_ui_id,
                      int32_t request_id,
                      CheckForCancelCallback callback) override;
  void SetAccessibilityTree(
      int32_t cookie,
      const ui::AXTreeUpdate& accessibility_tree) override;
#endif
#if BUILDFLAG(IS_ANDROID)
  void PdfWritingDone(int page_count) override;
#endif

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace headless

#endif  // COMPONENTS_PRINTING_BROWSER_HEADLESS_HEADLESS_PRINT_MANAGER_H_
