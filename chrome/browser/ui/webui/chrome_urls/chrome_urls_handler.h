// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROME_URLS_CHROME_URLS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROME_URLS_CHROME_URLS_HANDLER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/webui/chrome_urls/mojom/chrome_urls.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class BrowserContext;
}

namespace chrome_urls {

// Page handler for chrome://chrome-urls
class ChromeUrlsHandler : public chrome_urls::mojom::PageHandler {
 public:
  ChromeUrlsHandler(
      mojo::PendingReceiver<chrome_urls::mojom::PageHandler> receiver,
      mojo::PendingRemote<chrome_urls::mojom::Page> page,
      content::BrowserContext* browser_context);
  ~ChromeUrlsHandler() override;
  ChromeUrlsHandler(const ChromeUrlsHandler&) = delete;
  ChromeUrlsHandler& operator=(const ChromeUrlsHandler&) = delete;

 private:
  // chrome_urls::mojom::PageHandler
  void GetUrls(GetUrlsCallback callback) override;
  void SetDebugPagesEnabled(bool enabled,
                            SetDebugPagesEnabledCallback callback) override;
  FRIEND_TEST_ALL_PREFIXES(ChromeUrlsHandlerTest, GetUrls);
  FRIEND_TEST_ALL_PREFIXES(ChromeUrlsHandlerTest, SetDebugPagesEnabled);

  // These are located at the end of the list of member variables to ensure the
  // WebUI page is disconnected before other members are destroyed.
  mojo::Receiver<chrome_urls::mojom::PageHandler> receiver_;
  mojo::Remote<chrome_urls::mojom::Page> page_;
  raw_ptr<content::BrowserContext> browser_context_;
};

}  // namespace chrome_urls

#endif  // CHROME_BROWSER_UI_WEBUI_CHROME_URLS_CHROME_URLS_HANDLER_H_
