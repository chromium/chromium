// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_PDF_OCR_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_PDF_OCR_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace settings {

// Settings handler for the PDF OCR settings subpage
class PdfOcrHandler : public SettingsPageUIHandler,
                      public screen_ai::ScreenAIInstallState::Observer {
 public:
  PdfOcrHandler();

  PdfOcrHandler(const PdfOcrHandler&) = delete;
  PdfOcrHandler& operator=(const PdfOcrHandler&) = delete;

  ~PdfOcrHandler() override;

  // SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // screen_ai::ScreenAIInstallState::Observer:
  void DownloadProgressChanged(double progress) override;
  void StateChanged(screen_ai::ScreenAIInstallState::State state) override;

  void HandlePdfOcrSectionReady(const base::Value::List& args);

 private:
  base::ScopedObservation<screen_ai::ScreenAIInstallState,
                          screen_ai::ScreenAIInstallState::Observer>
      component_ready_observer{this};

  base::WeakPtrFactory<PdfOcrHandler> weak_factory_{this};
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_PDF_OCR_HANDLER_H_
