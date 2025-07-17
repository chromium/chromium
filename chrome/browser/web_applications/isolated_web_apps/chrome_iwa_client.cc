// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/chrome_iwa_client.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"

namespace web_app {

void ChromeIwaClient::CreateSingleton() {
  static base::NoDestructor<ChromeIwaClient> instance;
  instance.get();
}

base::expected<void, std::string> ChromeIwaClient::ValidateTrust(
    content::BrowserContext* browser_context,
    const web_package::SignedWebBundleId& web_bundle_id,
    bool dev_mode) {
  return IsolatedWebAppTrustChecker::IsTrusted(
      *Profile::FromBrowserContext(browser_context), web_bundle_id, dev_mode);
}

base::expected<web_package::SignedWebBundleId, std::string>
ChromeIwaClient::CreateWebBundleIdFromURL(const GURL& url) {
  return IsolatedWebAppUrlInfo::Create(url).transform(
      [](const auto& url_info) { return url_info.web_bundle_id(); });
}

}  // namespace web_app
