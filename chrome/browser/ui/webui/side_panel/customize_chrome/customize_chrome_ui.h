// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome.mojom.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"

namespace content {
class WebContents;
}  // namespace content

class CustomizeChromePageHandler;
class Profile;

// WebUI controller for chrome://customize-chrome-side-panel.top-chrome
class CustomizeChromeUI
    : public ui::MojoBubbleWebUIController,
      public side_panel::mojom::CustomizeChromePageHandlerFactory {
 public:
  explicit CustomizeChromeUI(content::WebUI* web_ui);
  CustomizeChromeUI(const CustomizeChromeUI&) = delete;
  CustomizeChromeUI& operator=(const CustomizeChromeUI&) = delete;

  ~CustomizeChromeUI() override;

  // Instantiates the implementor of the
  // mojom::CustomizeChromePageHandler mojo interface passing the pending
  // receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<
          side_panel::mojom::CustomizeChromePageHandlerFactory> receiver);

 private:
  // side_panel::mojom::CustomizeChromePageHandlerFactory
  void CreatePageHandler(
      mojo::PendingRemote<side_panel::mojom::CustomizeChromePage> pending_page,
      mojo::PendingReceiver<side_panel::mojom::CustomizeChromePageHandler>
          pending_page_handler) override;

  std::unique_ptr<CustomizeChromePageHandler> customize_chrome_page_handler_;
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
  mojo::Receiver<side_panel::mojom::CustomizeChromePageHandlerFactory>
      page_factory_receiver_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_UI_H_
