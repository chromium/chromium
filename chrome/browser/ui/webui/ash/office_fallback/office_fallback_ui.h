// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_OFFICE_FALLBACK_OFFICE_FALLBACK_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_OFFICE_FALLBACK_OFFICE_FALLBACK_UI_H_

#include <memory>

#include "ash/constants/ash_features.h"
#include "chrome/browser/ui/webui/ash/office_fallback/office_fallback.mojom-shared.h"
#include "chrome/browser/ui/webui/ash/office_fallback/office_fallback.mojom.h"
#include "chrome/browser/ui/webui/ash/office_fallback/office_fallback_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace ui {
class ColorChangeHandler;
}

namespace ash::office_fallback {

// The string conversions of ash::office_fallback::mojom::DialogChoice.
const char kDialogChoiceCancel[] = "cancel";
const char kDialogChoiceOk[] = "ok";
const char kDialogChoiceQuickOffice[] = "quick-office";
const char kDialogChoiceTryAgain[] = "try-again";

class OfficeFallbackUI;

// WebUIConfig for chrome://office-fallback.
class OfficeFallbackUIConfig
    : public content::DefaultWebUIConfig<OfficeFallbackUI> {
 public:
  OfficeFallbackUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIOfficeFallbackHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The UI for chrome://office-fallback, used for allowing the user to chose what
// to do when opening an office file fails.
class OfficeFallbackUI : public ui::MojoWebDialogUI,
                         public mojom::PageHandlerFactory {
 public:
  explicit OfficeFallbackUI(content::WebUI* web_ui);
  OfficeFallbackUI(const OfficeFallbackUI&) = delete;
  OfficeFallbackUI& operator=(const OfficeFallbackUI&) = delete;

  ~OfficeFallbackUI() override;

  // Instantiates implementor of the mojom::PageHandlerFactory
  // mojo interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<mojom::PageHandlerFactory> pending_receiver);

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

  // mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<mojom::PageHandler> pending_page_handler) override;

 private:
  void CloseDialog(mojom::DialogChoice choice);

  std::unique_ptr<OfficeFallbackPageHandler> page_handler_;
  mojo::Receiver<mojom::PageHandlerFactory> factory_receiver_{this};
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash::office_fallback

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_OFFICE_FALLBACK_OFFICE_FALLBACK_UI_H_
