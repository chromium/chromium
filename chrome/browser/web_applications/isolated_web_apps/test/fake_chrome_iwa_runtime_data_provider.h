// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_FAKE_CHROME_IWA_RUNTIME_DATA_PROVIDER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_FAKE_CHROME_IWA_RUNTIME_DATA_PROVIDER_H_

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/observer_list.h"
#include "base/one_shot_event.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/proto/key_distribution.pb.h"
#include "chrome/browser/web_applications/isolated_web_apps/runtime_data/chrome_iwa_runtime_data_provider.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/public/iwa_runtime_data_provider.h"

namespace web_app {

class FakeIwaRuntimeDataProviderBase : public ChromeIwaRuntimeDataProvider {
 public:
  FakeIwaRuntimeDataProviderBase();
  ~FakeIwaRuntimeDataProviderBase() override;

  // ChromeIwaRuntimeDataProvider:
  base::CallbackListSubscription OnRuntimeDataChanged(
      base::RepeatingClosure callback) override;
  base::OneShotEvent& OnBestEffortRuntimeDataReady() override;
  void WriteDebugMetadata(base::Value::Dict& log) const override;

 protected:
  void DispatchRuntimeDataUpdate();

 private:
  base::OneShotEvent event_;
  base::RepeatingClosureList subscriptions_;
};

class FakeIwaRuntimeDataProvider : public FakeIwaRuntimeDataProviderBase {
 public:
  FakeIwaRuntimeDataProvider();
  ~FakeIwaRuntimeDataProvider() override;

  const KeyRotationInfo* GetKeyRotationInfo(
      const std::string& web_bundle_id) const override;
  bool IsManagedInstallPermitted(std::string_view web_bundle_id) const override;
  bool IsManagedUpdatePermitted(std::string_view web_bundle_id) const override;
  bool IsBundleBlocklisted(std::string_view web_bundle_id) const override;
  const SpecialAppPermissionsInfo* GetSpecialAppPermissionsInfo(
      const std::string& web_bundle_id) const override;
  std::vector<std::string> GetSkipMultiCaptureNotificationBundleIds()
      const override;

  void SetManagedAllowlist(std::vector<web_package::SignedWebBundleId>);
  void RotateKey(const web_package::SignedWebBundleId& web_bundle_id,
                 base::span<const uint8_t> key_bytes);

 private:
  std::vector<web_package::SignedWebBundleId> managed_allowlist_;
  base::flat_map<std::string, IwaRuntimeDataProvider::KeyRotationInfo>
      key_rotations_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_FAKE_CHROME_IWA_RUNTIME_DATA_PROVIDER_H_
