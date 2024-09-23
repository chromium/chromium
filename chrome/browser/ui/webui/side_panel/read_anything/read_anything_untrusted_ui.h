// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_UNTRUSTED_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_UNTRUSTED_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/browser/ui/webui/top_chrome/untrusted_top_chrome_web_ui_controller.h"
#include "chrome/common/accessibility/read_anything.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

class ReadAnythingUntrustedPageHandler;
class ReadAnythingUntrustedUI;

class ReadAnythingUIUntrustedConfig
    : public DefaultTopChromeWebUIConfig<ReadAnythingUntrustedUI> {
 public:
  ReadAnythingUIUntrustedConfig();
  ~ReadAnythingUIUntrustedConfig() override;
};

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingUntrustedUI
//
//  A WebUI that holds distilled page contents for the Read Anything feature.
//  This class has the same lifetime as the Side Panel view.
//
class ReadAnythingUntrustedUI
    : public UntrustedTopChromeWebUIController,
      public read_anything::mojom::UntrustedPageHandlerFactory {
 public:
  explicit ReadAnythingUntrustedUI(content::WebUI* web_ui);
  ReadAnythingUntrustedUI(const ReadAnythingUntrustedUI&) = delete;
  ReadAnythingUntrustedUI& operator=(const ReadAnythingUntrustedUI&) = delete;
  ~ReadAnythingUntrustedUI() override;

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          pending_receiver);

  // Instantiates the implementor of the mojom::UntrustedPageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<read_anything::mojom::UntrustedPageHandlerFactory>
          receiver);

  static constexpr std::string GetWebUIName() {
    return "ReadAnythingUntrusted";
  }

  // read_anything::mojom::UntrustedPageHandlerFactory:
  void ShouldShowUI() override;

 private:
  void CreateUntrustedPageHandler(
      mojo::PendingRemote<read_anything::mojom::UntrustedPage> page,
      mojo::PendingReceiver<read_anything::mojom::UntrustedPageHandler>
          receiver) override;

  std::unique_ptr<ReadAnythingUntrustedPageHandler>
      read_anything_untrusted_page_handler_;
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;
  mojo::Receiver<read_anything::mojom::UntrustedPageHandlerFactory>
      read_anything_page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_UNTRUSTED_UI_H_
