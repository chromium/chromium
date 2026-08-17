// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_settings_impl.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/metrics/dwa/dwa_builders.h"
#include "components/metrics/dwa/dwa_recorder.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/site_for_cookies.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "url/gurl.h"
namespace privacy_sandbox {

PrivacySandboxSettingsImpl::PrivacySandboxSettingsImpl(
    HostContentSettingsMap* host_content_settings_map,
    scoped_refptr<content_settings::CookieSettings> cookie_settings,
    PrefService* pref_service)
    : host_content_settings_map_(host_content_settings_map),
      cookie_settings_(cookie_settings),
      pref_service_(pref_service) {
  CHECK(pref_service_);
  CHECK(host_content_settings_map_);
  CHECK(cookie_settings_);

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
      base::BindRepeating(
          &PrivacySandboxSettingsImpl::OnRelatedWebsiteSetsEnabledPrefChanged,
          base::Unretained(this)));
}

PrivacySandboxSettingsImpl::~PrivacySandboxSettingsImpl() = default;

void PrivacySandboxSettingsImpl::Shutdown() {
  observers_.Clear();
  host_content_settings_map_ = nullptr;
  cookie_settings_.reset();
  pref_service_ = nullptr;
  pref_change_registrar_.Reset();
}

bool PrivacySandboxSettingsImpl::IsEventReportingDestinationAttested(
    const url::Origin& destination_origin,
    privacy_sandbox::PrivacySandboxAttestationsGatedAPI invoking_api) const {
  return false;
}

bool PrivacySandboxSettingsImpl::IsSharedStorageAllowed(
    const url::Origin& top_frame_origin,
    const url::Origin& accessing_origin,
    std::string* out_debug_message,
    content::RenderFrameHost* console_frame,
    bool* out_block_is_site_setting_specific) const {
  return false;
}

bool PrivacySandboxSettingsImpl::IsSharedStorageSelectURLAllowed(
    const url::Origin& top_frame_origin,
    const url::Origin& accessing_origin,
    std::string* out_debug_message,
    bool* out_block_is_site_setting_specific) const {
  return false;
}

void PrivacySandboxSettingsImpl::OnRelatedWebsiteSetsEnabledPrefChanged() {
  for (auto& observer : observers_) {
    observer.OnRelatedWebsiteSetsEnabledChanged(AreRelatedWebsiteSetsEnabled());
  }
}

void PrivacySandboxSettingsImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PrivacySandboxSettingsImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool PrivacySandboxSettingsImpl::AreRelatedWebsiteSetsEnabled() const {
  return pref_service_->GetBoolean(
      prefs::kPrivacySandboxRelatedWebsiteSetsEnabled);
}

}  // namespace privacy_sandbox
