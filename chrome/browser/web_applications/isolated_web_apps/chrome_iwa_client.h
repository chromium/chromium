// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_CHROME_IWA_CLIENT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_CHROME_IWA_CLIENT_H_

#include "base/no_destructor.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/client.h"

namespace web_app {

class ChromeIwaClient : public IwaClient {
 public:
  // Creates a global singleton that can be accessed via
  // `web_app::IwaClient::GetInstance()`.
  static void CreateSingleton();

  // IwaClient:
  base::expected<void, std::string> ValidateTrust(
      content::BrowserContext* browser_context,
      const web_package::SignedWebBundleId& web_bundle_id,
      bool dev_mode) override;
  void RunWhenAppCloses(content::BrowserContext* browser_context,
                        const web_package::SignedWebBundleId& web_bundle_id,
                        base::OnceClosure callback) override;
  void GetIwaSourceForRequest(
      content::BrowserContext* browser_context,
      const web_package::SignedWebBundleId& web_bundle_id,
      const network::ResourceRequest& request,
      const std::optional<content::FrameTreeNodeId>& frame_tree_node,
      base::OnceCallback<void(
          base::expected<IwaSourceWithModeOrGeneratedResponse, std::string>)>
          callback) override;
  IwaRuntimeDataProvider* GetRuntimeDataProvider() override;

 private:
  ChromeIwaClient() = default;
  ~ChromeIwaClient() override = default;

  friend base::NoDestructor<ChromeIwaClient>;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_CHROME_IWA_CLIENT_H_
