// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_RELATED_WEBSITE_SETS_RELATED_WEBSITE_SETS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_RELATED_WEBSITE_SETS_RELATED_WEBSITE_SETS_HANDLER_H_

#include "chrome/browser/ui/webui/privacy_sandbox/related_website_sets/related_website_sets.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

class RelatedWebsiteSetsHandler
    : public related_website_sets::mojom::RelatedWebsiteSetsPageHandler {
 public:
  explicit RelatedWebsiteSetsHandler(
      mojo::PendingReceiver<
          related_website_sets::mojom::RelatedWebsiteSetsPageHandler> receiver);

  ~RelatedWebsiteSetsHandler() override;

  RelatedWebsiteSetsHandler(const RelatedWebsiteSetsHandler&) = delete;
  RelatedWebsiteSetsHandler& operator=(const RelatedWebsiteSetsHandler&) =
      delete;

 private:
  mojo::Receiver<related_website_sets::mojom::RelatedWebsiteSetsPageHandler>
      receiver_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_RELATED_WEBSITE_SETS_RELATED_WEBSITE_SETS_HANDLER_H_
