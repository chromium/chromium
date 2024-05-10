// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tpcd/metadata/common/manager_base.h"

#include "components/content_settings/core/common/content_settings_rules.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/host_indexed_content_settings.h"
#include "net/base/features.h"

namespace tpcd::metadata::common {
ManagerBase::ManagerBase() = default;
ManagerBase::~ManagerBase() = default;

ContentSetting ManagerBase::GetContentSetting(
    const Grants& grants,
    const GURL& third_party_url,
    const GURL& first_party_url,
    content_settings::SettingInfo* out_info) const {
  ContentSetting result = CONTENT_SETTING_BLOCK;

  if (base::FeatureList::IsEnabled(net::features::kTpcdMetadataGrants)) {
    if (absl::holds_alternative<content_settings::HostIndexedContentSettings>(
            grants)) {
      const content_settings::RuleEntry* found =
          absl::get<content_settings::HostIndexedContentSettings>(grants).Find(
              third_party_url, first_party_url);
      if (found) {
        result = content_settings::ValueToContentSetting(found->second.value);
        if (out_info) {
          out_info->SetAttributes(*found);
        }
      }
    } else {
      const ContentSettingPatternSource* found =
          content_settings::FindContentSetting(
              third_party_url, first_party_url,
              absl::get<ContentSettingsForOneType>(grants));
      if (found) {
        result = found->GetContentSetting();
        if (out_info) {
          out_info->SetAttributes(*found);
        }
      }
    }
  }

  if (out_info && result == CONTENT_SETTING_BLOCK) {
    out_info->primary_pattern = ContentSettingsPattern::Wildcard();
    out_info->secondary_pattern = ContentSettingsPattern::Wildcard();
    out_info->metadata = {};
  }
  return result;
}

ContentSettingsForOneType ManagerBase::GetContentSettingForOneType(
    const content_settings::HostIndexedContentSettings& grants) const {
  ContentSettingsForOneType result;
  for (const auto& RuleEntry : grants) {
    result.emplace_back(
        RuleEntry.first.primary_pattern, RuleEntry.first.secondary_pattern,
        RuleEntry.second.value.Clone(), content_settings::ProviderType::kNone,
        false, RuleEntry.second.metadata);
  }
  return result;
}
}  // namespace tpcd::metadata::common
