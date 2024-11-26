// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROME_URLS_CHROME_URLS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROME_URLS_CHROME_URLS_HANDLER_H_

#include "components/chrome_urls_ui/mojom/chrome_urls.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chrome_urls {

// Page handler for chrome://chrome-urls
class ChromeUrlsHandler : public chrome_urls::mojom::PageHandler {
 public:
  ChromeUrlsHandler(
      mojo::PendingReceiver<chrome_urls::mojom::PageHandler> receiver,
      mojo::PendingRemote<chrome_urls::mojom::Page> page);
  ~ChromeUrlsHandler() override;
  ChromeUrlsHandler(const ChromeUrlsHandler&) = delete;
  ChromeUrlsHandler& operator=(const ChromeUrlsHandler&) = delete;

 private:
  // chrome_urls::mojom::PageHandler
  void GetUrls(GetUrlsCallback callback) override;

  // These are located at the end of the list of member variables to ensure the
  // WebUI page is disconnected before other members are destroyed.
  mojo::Receiver<chrome_urls::mojom::PageHandler> receiver_;
  mojo::Remote<chrome_urls::mojom::Page> page_;
};

}  // namespace chrome_urls

#endif  // CHROME_BROWSER_UI_WEBUI_CHROME_URLS_CHROME_URLS_HANDLER_H_
