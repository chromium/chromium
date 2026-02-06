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
  void WriteDebugMetadata(base::DictValue& log) const override;

 protected:
  void DispatchRuntimeDataUpdate();

 private:
  base::OneShotEvent event_;
  base::RepeatingClosureList subscriptions_;
};

class FakeIwaRuntimeDataProvider : public FakeIwaRuntimeDataProviderBase {
 public:
  using ManagedAllowlist = std::vector<web_package::SignedWebBundleId>;
  using Blocklist = std::vector<web_package::SignedWebBundleId>;
  using KeyRotations = base::flat_map<std::string, KeyRotationInfo>;
  using SpecialPermissions =
      base::flat_map<std::string, SpecialAppPermissionsInfo>;
  using UserInstallAllowlist =
      base::flat_map<std::string, UserInstallAllowlistItemData>;

  class ScopedIwaRuntimeDataUpdate {
   public:
    explicit ScopedIwaRuntimeDataUpdate(FakeIwaRuntimeDataProvider&);
    ~ScopedIwaRuntimeDataUpdate();

    ScopedIwaRuntimeDataUpdate(const ScopedIwaRuntimeDataUpdate&) = delete;
    ScopedIwaRuntimeDataUpdate& operator=(const ScopedIwaRuntimeDataUpdate&) =
        delete;
    ScopedIwaRuntimeDataUpdate(ScopedIwaRuntimeDataUpdate&&) = delete;
    ScopedIwaRuntimeDataUpdate& operator=(ScopedIwaRuntimeDataUpdate&&) =
        delete;

    ScopedIwaRuntimeDataUpdate& AddToManagedAllowlist(
        const web_package::SignedWebBundleId& web_bundle_id);
    ScopedIwaRuntimeDataUpdate& SetManagedAllowlist(
        ManagedAllowlist managed_allowlist);

    ScopedIwaRuntimeDataUpdate& AddToKeyRotations(
        const web_package::SignedWebBundleId& web_bundle_id,
        base::span<const uint8_t> key_bytes,
        std::optional<base::span<const uint8_t>> previous_key_bytes =
            std::nullopt);
    ScopedIwaRuntimeDataUpdate& SetKeyRotations(KeyRotations key_rotations);

    ScopedIwaRuntimeDataUpdate& AddToSpecialPermissions(
        const web_package::SignedWebBundleId& web_bundle_id,
        const SpecialAppPermissionsInfo& info);
    ScopedIwaRuntimeDataUpdate& SetSpecialPermissions(
        SpecialPermissions special_permissions);

    ScopedIwaRuntimeDataUpdate& AddToBlocklist(
        const web_package::SignedWebBundleId& web_bundle_id);
    ScopedIwaRuntimeDataUpdate& SetBlocklist(Blocklist blocklist);

    ScopedIwaRuntimeDataUpdate& AddToUserInstallAllowlist(
        const web_package::SignedWebBundleId& web_bundle_id,
        const UserInstallAllowlistItemData& data);
    ScopedIwaRuntimeDataUpdate& SetUserInstallAllowlist(
        UserInstallAllowlist user_install_allowlist);

   private:
    ManagedAllowlist managed_allowlist_;
    Blocklist blocklist_;
    KeyRotations key_rotations_;
    SpecialPermissions special_permissions_;
    UserInstallAllowlist user_install_allowlist_;

    const raw_ref<FakeIwaRuntimeDataProvider> data_provider_;
  };

  FakeIwaRuntimeDataProvider();
  ~FakeIwaRuntimeDataProvider() override;

  const KeyRotationInfo* GetKeyRotationInfo(
      const std::string& web_bundle_id) const override;
  const UserInstallAllowlistItemData* GetUserInstallAllowlistData(
      const std::string& web_bundle_id) const override;
  bool IsManagedInstallPermitted(std::string_view web_bundle_id) const override;
  bool IsManagedUpdatePermitted(std::string_view web_bundle_id) const override;
  bool IsBundleBlocklisted(std::string_view web_bundle_id) const override;
  const SpecialAppPermissionsInfo* GetSpecialAppPermissionsInfo(
      const std::string& web_bundle_id) const override;
  std::vector<std::string> GetSkipMultiCaptureNotificationBundleIds()
      const override;

  // Takes care of creating a scoped update and applying the supplied functor to
  // it. All modifications will be applied at once when the object goes out of
  // scope.
  void Update(std::invocable<ScopedIwaRuntimeDataUpdate&> auto functor) {
    auto update = ScopedIwaRuntimeDataUpdate(*this);
    functor(update);
  }

 private:
  ManagedAllowlist managed_allowlist_;
  Blocklist blocklist_;
  KeyRotations key_rotations_;
  SpecialPermissions special_permissions_;
  UserInstallAllowlist user_install_allowlist_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_FAKE_CHROME_IWA_RUNTIME_DATA_PROVIDER_H_
