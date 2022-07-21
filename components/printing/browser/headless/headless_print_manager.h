// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_BROWSER_HEADLESS_HEADLESS_PRINT_MANAGER_H_
#define COMPONENTS_PRINTING_BROWSER_HEADLESS_HEADLESS_PRINT_MANAGER_H_

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

namespace headless {

// Minimalistic PrintManager implemementation intended for use with Headless
// Chrome. It shortcuts most of the methods exposing only PrintToPdf()
// functionality.
class HeadlessPrintManager
    : public printing::PrintManager,
      public content::WebContentsUserData<HeadlessPrintManager> {
 public:
  using PrintToPdfCallback =
      base::OnceCallback<void(print_to_pdf::PdfPrintResult,
                              scoped_refptr<base::RefCountedMemory>)>;

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
                  PrintToPdfCallback callback);

 private:
  friend class content::WebContentsUserData<HeadlessPrintManager>;

  explicit HeadlessPrintManager(content::WebContents* web_contents);

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
  void ReleaseJob(print_to_pdf::PdfPrintResult result);

  raw_ptr<content::RenderFrameHost> printing_rfh_ = nullptr;
  PrintToPdfCallback callback_;
  std::string data_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace headless

#endif  // COMPONENTS_PRINTING_BROWSER_HEADLESS_HEADLESS_PRINT_MANAGER_H_
