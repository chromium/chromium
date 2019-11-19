// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEED_INTERNALS_FEED_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_FEED_INTERNALS_FEED_INTERNALS_UI_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/webui/feed_internals/feed_internals.mojom.h"
#include "chrome/browser/ui/webui/feed_internals/feed_internals_page_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class Profile;

// During the interim migration to Feed, this page will be co-located with
// snippets-internals. Once migration is complete, and snippets-internals is
// removed, this page will be moved to chrome://feed-internals.

// UI controller for the Feed internals page, hooks up a concrete implementation
// of feed_internals::mojom::PageHandler to requests for that page handler
// that will come from the frontend.
class FeedInternalsUI : public ui::MojoWebUIController {
 public:
  explicit FeedInternalsUI(content::WebUI* web_ui);
  ~FeedInternalsUI() override;

 private:
  void BindFeedInternalsPageHandler(
      mojo::PendingReceiver<feed_internals::mojom::PageHandler> receiver);

  Profile* profile_;

  std::unique_ptr<FeedInternalsPageHandler> page_handler_;

  DISALLOW_COPY_AND_ASSIGN(FeedInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEED_INTERNALS_FEED_INTERNALS_UI_H_
