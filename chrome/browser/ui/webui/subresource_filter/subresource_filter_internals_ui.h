// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_INTERNALS_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/subresource_filter/subresource_filter_internals.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/internal_webui_config.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace subresource_filter {

class SubresourceFilterInternalsUI;
class SubresourceFilterInternalsHandler;

class SubresourceFilterInternalsUIConfig
    : public content::DefaultInternalWebUIConfig<SubresourceFilterInternalsUI> {
 public:
  SubresourceFilterInternalsUIConfig()
      : DefaultInternalWebUIConfig(
            chrome::kChromeUISubresourceFilterInternalsHost) {}
};

// The WebUI controller for chrome://subresource-filter-internals.
// Handles initialization and configuration of the internals page data source.
class SubresourceFilterInternalsUI : public ui::MojoWebUIController {
 public:
  explicit SubresourceFilterInternalsUI(content::WebUI* web_ui);

  SubresourceFilterInternalsUI(const SubresourceFilterInternalsUI&) = delete;
  SubresourceFilterInternalsUI& operator=(const SubresourceFilterInternalsUI&) =
      delete;

  ~SubresourceFilterInternalsUI() override;

  void BindInterface(
      mojo::PendingReceiver<mojom::SubresourceFilterInternalsHandler> receiver);

 private:
  std::unique_ptr<SubresourceFilterInternalsHandler> handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace subresource_filter

#endif  // CHROME_BROWSER_UI_WEBUI_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_INTERNALS_UI_H_
