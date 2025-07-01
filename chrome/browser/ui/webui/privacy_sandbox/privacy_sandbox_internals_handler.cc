// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_internals_handler.h"

#include "base/logging.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_internals.mojom.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings_pattern_parser.h"
#include "privacy_sandbox_internals_handler.h"

namespace privacy_sandbox_internals {
using ::content_settings::WebsiteSettingsRegistry;

PrivacySandboxInternalsHandler::PrivacySandboxInternalsHandler(
    Profile* profile,
    mojo::PendingReceiver<privacy_sandbox_internals::mojom::PageHandler>
        pending_receiver)
    : profile_(profile) {
  receiver_.Bind(std::move(pending_receiver));
}

PrivacySandboxInternalsHandler::~PrivacySandboxInternalsHandler() = default;

// TODO(crbug.com/427457772): Evaluate making this function more performant by
// extending the PrefService API to read by prefix.
// TODO(crbug.com/427990026): Optimize performance of this with a trie.
//
// This method potentially allocates MBs of memory and should not be used
// outside of internals pages.
void PrivacySandboxInternalsHandler::ReadPrefsWithPrefixes(
    const std::vector<std::string>& pref_prefixes,
    ReadPrefsWithPrefixesCallback callback) {
  // Empty strings and duplicates in the prefix list are considered invalid.
  base::flat_set<std::string> prefixes_set(pref_prefixes);
  if (prefixes_set.size() != pref_prefixes.size()) {
    receiver_.ReportBadMessage("Duplicate prefixes are invalid.");
    return;
  }

  if (prefixes_set.contains("")) {
    receiver_.ReportBadMessage("Empty prefixes are invalid.");
    return;
  }

  std::vector<PrefService::PreferenceValueAndStore> values =
      profile_->GetPrefs()->GetPreferencesValueAndStore();

  std::vector<privacy_sandbox_internals::mojom::PrivacySandboxInternalsPrefPtr>
      filteredPrefs;
  for (auto& [name, value, pref_value_store_type] : values) {
    for (auto prefix : pref_prefixes) {
      if (name.starts_with(prefix)) {
        auto pref =
            privacy_sandbox_internals::mojom::PrivacySandboxInternalsPref::New(
                name, std::move(value));
        filteredPrefs.push_back(std::move(pref));
        break;
      }
    }
  }

  std::move(callback).Run(std::move(filteredPrefs));
}

void PrivacySandboxInternalsHandler::ReadContentSettings(
    const ContentSettingsType type,
    ReadContentSettingsCallback callback) {
  if (!IsKnownEnumValue(type)) {
    mojo::ReportBadMessage(
        "ReadContentSettings received invalid ContentSettingsType");
    return;
  }

  // HostContentSettingsMap will assert if we attempt to read unregistered
  // content types, so for these types we simply return an empty list.
  if (WebsiteSettingsRegistry::GetInstance()->Get(type) == nullptr) {
    std::move(callback).Run({});
    return;
  }

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  std::move(callback).Run(map->GetSettingsForOneType(type));
}

void PrivacySandboxInternalsHandler::GetTpcdMetadataGrants(
    GetTpcdMetadataGrantsCallback callback) {
  content_settings::CookieSettings* cookie_settings =
      CookieSettingsFactory::GetForProfile(profile_).get();
  std::move(callback).Run(cookie_settings->GetTpcdMetadataGrants());
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
