// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_internals_handler.h"
#include "base/logging.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern_parser.h"
#include "privacy_sandbox_internals_handler.h"

namespace privacy_sandbox_internals {

PrivacySandboxInternalsHandler::PrivacySandboxInternalsHandler(
    Profile* profile,
    mojo::PendingReceiver<privacy_sandbox_internals::mojom::PageHandler>
        pending_receiver)
    : profile_(profile) {
  receiver_.Bind(std::move(pending_receiver));
}

PrivacySandboxInternalsHandler::~PrivacySandboxInternalsHandler() = default;

void PrivacySandboxInternalsHandler::ReadPref(const std::string& pref_name,
                                              ReadPrefCallback callback) {
  const PrefService::Preference* pref =
      profile_->GetPrefs()->FindPreference(pref_name);
  if (pref) {
    std::move(callback).Run(pref->GetValue()->Clone());
    return;
  }

  // If the pref isn't registered we return a null Value.
  std::move(callback).Run(base::Value());
}

void PrivacySandboxInternalsHandler::GetCookieSettings(
    GetCookieSettingsCallback callback) {
  content_settings::CookieSettings* cookie_settings =
      CookieSettingsFactory::GetForProfile(profile_).get();
  std::move(callback).Run(cookie_settings->GetCookieSettings());
}

void PrivacySandboxInternalsHandler::GetTpcdMetadataGrants(
    GetTpcdMetadataGrantsCallback callback) {
  content_settings::CookieSettings* cookie_settings =
      CookieSettingsFactory::GetForProfile(profile_).get();
  std::move(callback).Run(cookie_settings->GetTpcdMetadataGrants());
}

void PrivacySandboxInternalsHandler::GetTpcdHeuristicsGrants(
    GetTpcdMetadataGrantsCallback callback) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  std::move(callback).Run(
      map->GetSettingsForOneType(ContentSettingsType::TPCD_HEURISTICS_GRANTS));
}

void PrivacySandboxInternalsHandler::GetTpcdTrial(
    GetTpcdMetadataGrantsCallback callback) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  std::move(callback).Run(
      map->GetSettingsForOneType(ContentSettingsType::TPCD_TRIAL));
}

void PrivacySandboxInternalsHandler::GetTopLevelTpcdTrial(
    GetTpcdMetadataGrantsCallback callback) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  std::move(callback).Run(
      map->GetSettingsForOneType(ContentSettingsType::TOP_LEVEL_TPCD_TRIAL));
}

void PrivacySandboxInternalsHandler::ContentSettingsPatternToString(
    const ContentSettingsPattern& pattern,
    ContentSettingsPatternToStringCallback callback) {
  std::move(callback).Run(pattern.ToString());
}

void PrivacySandboxInternalsHandler::StringToContentSettingsPattern(
    const std::string& s,
    StringToContentSettingsPatternCallback callback) {
  std::move(callback).Run(ContentSettingsPattern::FromString(s));
}

}  // namespace privacy_sandbox_internals
