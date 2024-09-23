// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BROWSING_TOPICS_BROWSING_TOPICS_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_BROWSING_TOPICS_BROWSING_TOPICS_INTERNALS_UI_H_

#include <memory>

#include "chrome/common/webui_url_constants.h"
#include "components/browsing_topics/mojom/browsing_topics_internals.mojom-forward.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class BrowsingTopicsInternalsPageHandler;
class BrowsingTopicsInternalsUI;

// WebUIConfig for chrome://browsing-topics-internals
class BrowsingTopicsInternalsUIConfig
    : public content::DefaultWebUIConfig<BrowsingTopicsInternalsUI> {
 public:
  BrowsingTopicsInternalsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIBrowsingTopicsInternalsHost) {}
};

// WebUI which handles serving the chrome://topics-internals page.
class BrowsingTopicsInternalsUI : public ui::MojoWebUIController {
 public:
  explicit BrowsingTopicsInternalsUI(content::WebUI* web_ui);
  BrowsingTopicsInternalsUI(const BrowsingTopicsInternalsUI&) = delete;
  BrowsingTopicsInternalsUI& operator=(const BrowsingTopicsInternalsUI&) =
      delete;
  ~BrowsingTopicsInternalsUI() override;

  // Instantiates the implementor of the mojom::PageHandler mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<browsing_topics::mojom::PageHandler> receiver);

  BrowsingTopicsInternalsPageHandler* page_handler() {
    return page_handler_.get();
  }

 private:
  std::unique_ptr<BrowsingTopicsInternalsPageHandler> page_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_BROWSING_TOPICS_BROWSING_TOPICS_INTERNALS_UI_H_
