// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/runtime_init.h"

#include "chrome/browser/web_applications/isolated_web_apps/chrome_iwa_client.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/iwa_key_distribution_info_provider.h"
#include "chrome/browser/web_applications/isolated_web_apps/runtime_data/chrome_iwa_runtime_data_provider.h"
#include "components/webapps/isolated_web_apps/identity/iwa_identity_validator.h"

namespace web_app {

void InitializeIsolatedWebAppRuntime(
    base::PassKey<BrowserProcessImpl, TestingBrowserProcess> pass_key) {
  web_app::IwaIdentityValidator::CreateSingleton();
  web_app::ChromeIwaClient::CreateSingleton();
  web_app::ChromeIwaRuntimeDataProvider::SetInstance(
      pass_key, &IwaKeyDistributionInfoProvider::GetInstance(pass_key));
}

}  // namespace web_app
