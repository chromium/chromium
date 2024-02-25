// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_UI_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_UI_H_

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "components/omnibox/browser/omnibox.mojom-forward.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom-forward.h"
#include "ui/webui/resources/js/metrics_reporter/metrics_reporter.mojom-forward.h"

class Profile;
class RealboxHandler;

namespace ui {
class ColorChangeHandler;
}  // namespace ui

// The Web UI controller for the chrome://omnibox-popup.top-chrome.
class OmniboxPopupUI : public ui::MojoWebUIController {
 public:
  explicit OmniboxPopupUI(content::WebUI* web_ui);
  OmniboxPopupUI(const OmniboxPopupUI&) = delete;
  OmniboxPopupUI& operator=(const OmniboxPopupUI&) = delete;
  ~OmniboxPopupUI() override;

  // Instantiates the implementor of the omnibox::mojom::PageHandler mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      content::RenderFrameHost* host,
      mojo::PendingReceiver<omnibox::mojom::PageHandler> pending_page_handler);
  // Instantiates the implementor of metrics_reporter::mojom::PageMetricsHost
  // mojo interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<metrics_reporter::mojom::PageMetricsHost> receiver);
  // Instantiates the implementor of color_change_listener::mojom::PageHandler
  // mojo interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          pending_receiver);

  RealboxHandler* handler() { return handler_.get(); }

 private:
  std::unique_ptr<RealboxHandler> handler_;
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;
  raw_ptr<Profile> profile_;
  MetricsReporter metrics_reporter_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_UI_H_
