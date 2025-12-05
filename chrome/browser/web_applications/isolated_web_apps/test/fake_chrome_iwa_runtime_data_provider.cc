// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/test/fake_chrome_iwa_runtime_data_provider.h"

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/map_util.h"
#include "base/containers/to_vector.h"
#include "base/notimplemented.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/public/iwa_runtime_data_provider.h"

namespace web_app {

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
    base::Value::Dict& log) const {}

void FakeIwaRuntimeDataProviderBase::DispatchRuntimeDataUpdate() {
  subscriptions_.Notify();
}

FakeIwaRuntimeDataProvider::FakeIwaRuntimeDataProvider() = default;
FakeIwaRuntimeDataProvider::~FakeIwaRuntimeDataProvider() = default;

const IwaRuntimeDataProvider::KeyRotationInfo*
FakeIwaRuntimeDataProvider::GetKeyRotationInfo(
    const std::string& web_bundle_id) const {
  return base::FindOrNull(key_rotations_, web_bundle_id);
}

bool FakeIwaRuntimeDataProvider::IsManagedInstallPermitted(
    std::string_view web_bundle_id) const {
  return base::Contains(managed_allowlist_, web_bundle_id,
                        &web_package::SignedWebBundleId::id);
}

bool FakeIwaRuntimeDataProvider::IsManagedUpdatePermitted(
    std::string_view web_bundle_id) const {
  return base::Contains(managed_allowlist_, web_bundle_id,
                        &web_package::SignedWebBundleId::id);
}

bool FakeIwaRuntimeDataProvider::IsBundleBlocklisted(
    std::string_view web_bundle_id) const {
  return false;
}

const ChromeIwaRuntimeDataProvider::SpecialAppPermissionsInfo*
FakeIwaRuntimeDataProvider::GetSpecialAppPermissionsInfo(
    const std::string& web_bundle_id) const {
  return nullptr;
}

std::vector<std::string>
FakeIwaRuntimeDataProvider::GetSkipMultiCaptureNotificationBundleIds() const {
  return {};
}

void FakeIwaRuntimeDataProvider::SetManagedAllowlist(
    std::vector<web_package::SignedWebBundleId> managed_allowlist) {
  managed_allowlist_ = std::move(managed_allowlist);
  DispatchRuntimeDataUpdate();
}

void FakeIwaRuntimeDataProvider::RotateKey(
    const web_package::SignedWebBundleId& web_bundle_id,
    base::span<const uint8_t> key_bytes) {
  key_rotations_.emplace(
      web_bundle_id.id(),
      IwaRuntimeDataProvider::KeyRotationInfo(base::ToVector(key_bytes)));
  DispatchRuntimeDataUpdate();
}

}  // namespace web_app
