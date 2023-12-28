// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_provider.h"
#include "base/feature_list.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/features.h"

namespace content_settings {

std::unique_ptr<OwnedRule> ProviderInterface::GetRule(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    bool off_the_record,
    const PartitionKey& partition_key) const {
  // TODO(b/316530672): Remove default implementation when all providers are
  // implemented.
  auto it = GetRuleIterator(content_type, off_the_record, partition_key);
  while (it && it->HasNext()) {
    auto rule = it->Next();
    if (rule->primary_pattern.Matches(primary_url) &&
        rule->secondary_pattern.Matches(secondary_url) &&
        (base::FeatureList::IsEnabled(
             content_settings::features::kActiveContentSettingExpiry) ||
         rule->metadata.expiration().is_null() ||
         rule->metadata.expiration() >= base::Time::Now())) {
      return std::make_unique<OwnedRule>(
          std::move(rule->primary_pattern), std::move(rule->secondary_pattern),
          rule->TakeValue(), std::move(rule->metadata));
    }
  }
  return nullptr;
}

}  // namespace content_settings
