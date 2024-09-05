// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SET_TIME_SET_TIME_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SET_TIME_SET_TIME_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace ui {
class ColorChangeHandler;
}  // namespace ui

namespace ash {

class SetTimeUI;

// WebUIConfig for chrome://set-time
class SetTimeUIConfig : public content::DefaultWebUIConfig<SetTimeUI> {
 public:
  SetTimeUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUISetTimeHost) {}
};

// The WebUI for chrome://set-time.
class SetTimeUI : public ui::MojoWebDialogUI {
 public:
  explicit SetTimeUI(content::WebUI* web_ui);

  SetTimeUI(const SetTimeUI&) = delete;
  SetTimeUI& operator=(const SetTimeUI&) = delete;

  ~SetTimeUI() override;

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

 private:
  // The color change handler notifies the WebUI when the color provider
  // changes the color palette
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SET_TIME_SET_TIME_UI_H_
