// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/test/fake_chrome_iwa_runtime_data_provider.h"

#include <algorithm>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/map_util.h"
#include "base/containers/to_vector.h"
#include "chrome/browser/web_applications/isolated_web_apps/runtime_data/chrome_iwa_runtime_data_provider.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/public/iwa_runtime_data_provider.h"

namespace web_app {

namespace {
using ScopedIwaRuntimeDataUpdate =
    FakeIwaRuntimeDataProvider::ScopedIwaRuntimeDataUpdate;
}  // namespace

FakeIwaRuntimeDataProviderBase::FakeIwaRuntimeDataProviderBase() {
  event_.Signal();
}

FakeIwaRuntimeDataProviderBase::~FakeIwaRuntimeDataProviderBase() = default;

base::OneShotEvent&
FakeIwaRuntimeDataProviderBase::OnBestEffortRuntimeDataReady() {
  return event_;
}

base::CallbackListSubscription
FakeIwaRuntimeDataProviderBase::OnRuntimeDataChanged(
    base::RepeatingClosure callback) {
  return subscriptions_.Add(std::move(callback));
}

void FakeIwaRuntimeDataProviderBase::WriteDebugMetadata(
    base::DictValue& log) const {}

void FakeIwaRuntimeDataProviderBase::DispatchRuntimeDataUpdate() {
  subscriptions_.Notify();
}

ScopedIwaRuntimeDataUpdate::ScopedIwaRuntimeDataUpdate(
    FakeIwaRuntimeDataProvider& provider)
    : managed_allowlist_(provider.managed_allowlist_),
      blocklist_(provider.blocklist_),
      key_rotations_(provider.key_rotations_),
      special_permissions_(provider.special_permissions_),
      user_install_allowlist_(provider.user_install_allowlist_),
      data_provider_(provider) {}

ScopedIwaRuntimeDataUpdate::~ScopedIwaRuntimeDataUpdate() {
  data_provider_->managed_allowlist_ = std::move(managed_allowlist_);
  data_provider_->blocklist_ = std::move(blocklist_);
  data_provider_->key_rotations_ = std::move(key_rotations_);
  data_provider_->special_permissions_ = std::move(special_permissions_);
  data_provider_->user_install_allowlist_ = std::move(user_install_allowlist_);
  data_provider_->DispatchRuntimeDataUpdate();
}

ScopedIwaRuntimeDataUpdate& ScopedIwaRuntimeDataUpdate::AddToManagedAllowlist(
    const web_package::SignedWebBundleId& web_bundle_id) {
  managed_allowlist_.push_back(web_bundle_id);
  return *this;
}

ScopedIwaRuntimeDataUpdate& ScopedIwaRuntimeDataUpdate::SetManagedAllowlist(
    ManagedAllowlist managed_allowlist) {
  managed_allowlist_ = std::move(managed_allowlist);
  return *this;
}

ScopedIwaRuntimeDataUpdate& ScopedIwaRuntimeDataUpdate::AddToBlocklist(
    const web_package::SignedWebBundleId& web_bundle_id) {
  blocklist_.push_back(web_bundle_id);
  return *this;
}

ScopedIwaRuntimeDataUpdate& ScopedIwaRuntimeDataUpdate::SetBlocklist(
    Blocklist blocklist) {
  blocklist_ = std::move(blocklist);
  return *this;
}

ScopedIwaRuntimeDataUpdate& ScopedIwaRuntimeDataUpdate::AddToKeyRotations(
    const web_package::SignedWebBundleId& web_bundle_id,
    base::span<const uint8_t> key_bytes,
    std::optional<base::span<const uint8_t>> previous_key_bytes) {
  std::optional<KeyRotationInfo::PublicKeyData> previous_key;
  if (previous_key_bytes) {
    previous_key = base::ToVector(*previous_key_bytes);
  }
  key_rotations_.insert_or_assign(
      web_bundle_id.id(),
      KeyRotationInfo(base::ToVector(key_bytes), std::move(previous_key)));
  return *this;
}

ScopedIwaRuntimeDataUpdate& ScopedIwaRuntimeDataUpdate::SetKeyRotations(
    KeyRotations key_rotations) {
  key_rotations_ = std::move(key_rotations);
  return *this;
}

ScopedIwaRuntimeDataUpdate& ScopedIwaRuntimeDataUpdate::AddToSpecialPermissions(
    const web_package::SignedWebBundleId& web_bundle_id,
    const SpecialAppPermissionsInfo& info) {
  special_permissions_.insert_or_assign(web_bundle_id.id(), info);
  return *this;
}

ScopedIwaRuntimeDataUpdate& ScopedIwaRuntimeDataUpdate::SetSpecialPermissions(
    SpecialPermissions special_permissions) {
  special_permissions_ = std::move(special_permissions);
  return *this;
}

ScopedIwaRuntimeDataUpdate&
ScopedIwaRuntimeDataUpdate::AddToUserInstallAllowlist(
    const web_package::SignedWebBundleId& web_bundle_id,
    const UserInstallAllowlistItemData& data) {
  user_install_allowlist_.insert_or_assign(web_bundle_id.id(), data);
  return *this;
}

ScopedIwaRuntimeDataUpdate& ScopedIwaRuntimeDataUpdate::SetUserInstallAllowlist(
    UserInstallAllowlist user_install_allowlist) {
  user_install_allowlist_ = std::move(user_install_allowlist);
  return *this;
}

FakeIwaRuntimeDataProvider::FakeIwaRuntimeDataProvider() = default;
FakeIwaRuntimeDataProvider::~FakeIwaRuntimeDataProvider() = default;

const ChromeIwaRuntimeDataProvider::KeyRotationInfo*
FakeIwaRuntimeDataProvider::GetKeyRotationInfo(
    const std::string& web_bundle_id) const {
  return base::FindOrNull(key_rotations_, web_bundle_id);
}

const ChromeIwaRuntimeDataProvider::UserInstallAllowlistItemData*
FakeIwaRuntimeDataProvider::GetUserInstallAllowlistData(
    const std::string& web_bundle_id) const {
  return base::FindOrNull(user_install_allowlist_, web_bundle_id);
}

bool FakeIwaRuntimeDataProvider::IsManagedInstallPermitted(
    std::string_view web_bundle_id) const {
  return std::ranges::contains(managed_allowlist_, web_bundle_id,
                               &web_package::SignedWebBundleId::id);
}

bool FakeIwaRuntimeDataProvider::IsManagedUpdatePermitted(
    std::string_view web_bundle_id) const {
  return std::ranges::contains(managed_allowlist_, web_bundle_id,
                               &web_package::SignedWebBundleId::id);
}

bool FakeIwaRuntimeDataProvider::IsBundleBlocklisted(
    std::string_view web_bundle_id) const {
  return std::ranges::contains(blocklist_, web_bundle_id,
                               &web_package::SignedWebBundleId::id);
}

const ChromeIwaRuntimeDataProvider::SpecialAppPermissionsInfo*
FakeIwaRuntimeDataProvider::GetSpecialAppPermissionsInfo(
    const std::string& web_bundle_id) const {
  return base::FindOrNull(special_permissions_, web_bundle_id);
}

std::vector<std::string>
FakeIwaRuntimeDataProvider::GetSkipMultiCaptureNotificationBundleIds() const {
  std::vector<std::string> bundle_ids;
  for (const auto& [bundle_id, permissions] : special_permissions_) {
    if (permissions.skip_capture_started_notification) {
      bundle_ids.push_back(bundle_id);
    }
  }
  return bundle_ids;
}

}  // namespace web_app
