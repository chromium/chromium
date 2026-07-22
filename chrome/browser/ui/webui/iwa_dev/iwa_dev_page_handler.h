// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_IWA_DEV_IWA_DEV_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_IWA_DEV_IWA_DEV_PAGE_HANDLER_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/webui/iwa_dev/iwa_dev.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace content {
class WebUI;
}  // namespace content

namespace web_app {
class WebAppProvider;
}  // namespace web_app

// Handles API requests from chrome://iwa-dev page by implementing
// iwa_dev::mojom::PageHandler.
class IwaDevPageHandler : public iwa_dev::mojom::PageHandler {
 public:
  IwaDevPageHandler(
      content::WebUI* web_ui,
      mojo::PendingReceiver<iwa_dev::mojom::PageHandler> receiver);

  IwaDevPageHandler(const IwaDevPageHandler&) = delete;
  IwaDevPageHandler& operator=(const IwaDevPageHandler&) = delete;

  ~IwaDevPageHandler() override;

  // iwa_dev::mojom::PageHandler:
  void GetInstalledAppsInfo(GetInstalledAppsInfoCallback callback) override;

 private:
  const raw_ref<Profile> profile_;
  const raw_ref<web_app::WebAppProvider> provider_;
  mojo::Receiver<iwa_dev::mojom::PageHandler> receiver_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_IWA_DEV_IWA_DEV_PAGE_HANDLER_H_
