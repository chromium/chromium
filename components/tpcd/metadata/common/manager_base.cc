// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tpcd/metadata/common/manager_base.h"

#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_rules.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/host_indexed_content_settings.h"
#include "net/base/features.h"

namespace tpcd::metadata::common {
ManagerBase::ManagerBase() = default;
ManagerBase::~ManagerBase() = default;

// Whether to bypass any available grants from the Third Party Cookie
// Deprecation TPCD Metadata.
bool IgnoreTpcdDtGracePeriodMetadataGrant(
    const content_settings::SettingInfo* info) {
  switch (info->metadata.tpcd_metadata_cohort()) {
    case content_settings::mojom::TpcdMetadataCohort::GRACE_PERIOD_FORCED_OFF:
      return true;
    case content_settings::mojom::TpcdMetadataCohort::DEFAULT:
    case content_settings::mojom::TpcdMetadataCohort::GRACE_PERIOD_FORCED_ON:
      return false;
  }

  NOTREACHED() << "Invalid enum value: "
               << info->metadata.tpcd_metadata_cohort();
}

ContentSetting ManagerBase::GetContentSetting(
    const content_settings::HostIndexedContentSettings& grants,
    const GURL& third_party_url,
    const GURL& first_party_url,
    content_settings::SettingInfo* out_info) const {
  ContentSetting result = CONTENT_SETTING_BLOCK;

  if (base::FeatureList::IsEnabled(net::features::kTpcdMetadataGrants)) {
    const content_settings::RuleEntry* found =
        grants.Find(third_party_url, first_party_url);
    if (found) {
      result = content_settings::ValueToContentSetting(found->second.value);
      if (out_info) {
        out_info->SetAttributes(*found);
      }
    }
  }

  // The `first_party_url` and `third_party_url` wasn't granted access by any of
  // the available metadata entries.
  if (out_info && result == CONTENT_SETTING_BLOCK) {
    out_info->primary_pattern = ContentSettingsPattern::Wildcard();
    out_info->secondary_pattern = ContentSettingsPattern::Wildcard();
    out_info->metadata = {};
  }

  // The `first_party_url` and `third_party_url` was granted access by at least
  // one of the available metadata entries, but shouldn't be considered as its
  // grace period is forced off.
  else if (out_info && IgnoreTpcdDtGracePeriodMetadataGrant(out_info)) {
    result = CONTENT_SETTING_BLOCK;
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
