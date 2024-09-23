// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEED_INTERNALS_FEED_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_FEED_INTERNALS_FEED_INTERNALS_UI_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/feed_internals/feed_internals.mojom-forward.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class Profile;
class FeedV2InternalsPageHandler;
class FeedInternalsUI;

class FeedInternalsUIConfig
    : public content::DefaultWebUIConfig<FeedInternalsUI> {
 public:
  FeedInternalsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUISnippetsInternalsHost) {}

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// During the interim migration to Feed, this page will be co-located with
// snippets-internals. Once migration is complete, and snippets-internals is
// removed, this page will be moved to chrome://feed-internals.

// UI controller for the Feed internals page, hooks up a concrete implementation
// of feed_internals::mojom::PageHandler to requests for that page handler
// that will come from the frontend.
class FeedInternalsUI : public ui::MojoWebUIController {
 public:
  explicit FeedInternalsUI(content::WebUI* web_ui);

  FeedInternalsUI(const FeedInternalsUI&) = delete;
  FeedInternalsUI& operator=(const FeedInternalsUI&) = delete;

  ~FeedInternalsUI() override;

  // Instantiates the implementor of the feed_internals::mojom::PageHandler mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<feed_internals::mojom::PageHandler> receiver);

 private:
  raw_ptr<Profile> profile_;
  std::unique_ptr<FeedV2InternalsPageHandler> v2_page_handler_;
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEED_INTERNALS_FEED_INTERNALS_UI_H_
