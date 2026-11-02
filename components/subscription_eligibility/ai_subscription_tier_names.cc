// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subscription_eligibility/ai_subscription_tier_names.h"

#include <optional>
#include <string>

#include "base/containers/map_util.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "components/subscription_eligibility/ai_subscription_tier_name_types.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "components/internal/subscription_eligibility/ai_subscription_tier_names_branded.h"
#else
#include "components/subscription_eligibility/ai_subscription_tier_names_specs.h"
#endif

namespace subscription_eligibility {

namespace {

// Returns the short name override for `tier`, or std::nullopt if the override
// feature is disabled, the mapping is malformed or has no entry for `tier`.
std::optional<std::u16string> FindShortNameOverride(int tier) {
  if (!base::FeatureList::IsEnabled(kEnabledAiSubscriptionTierNameOverride)) {
    return std::nullopt;
  }
  std::optional<base::DictValue> dict = base::JSONReader::ReadDict(
      kAiSubscriptionTierNameOverrideMapping.Get(), base::JSON_PARSE_RFC);
  if (!dict) {
    return std::nullopt;
  }
  const std::string* name = dict->FindString(base::NumberToString(tier));
  if (!name) {
    return std::nullopt;
  }
  return base::UTF8ToUTF16(*name);
}

}  // namespace

BASE_FEATURE(kEnabledAiSubscriptionTierNameOverride,
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kAiSubscriptionTierNameOverrideMapping{
    &kEnabledAiSubscriptionTierNameOverride, "ai_tier_override_mapping", "{}"};

std::u16string GetAiSubscriptionTierShortName(int tier) {
  if (std::optional<std::u16string> name = FindShortNameOverride(tier)) {
    return *name;
  }
  const AiTierName* tier_name = base::FindOrNull(kAiTierNames, tier);
  return tier_name ? std::u16string(tier_name->short_name) : std::u16string();
}

std::u16string GetAiSubscriptionTierFullName(int tier) {
  // An overridden short name would not match the built-in full name: let the
  // caller use a generic string instead.
  if (FindShortNameOverride(tier)) {
    return std::u16string();
  }
  const AiTierName* tier_name = base::FindOrNull(kAiTierNames, tier);
  return tier_name ? std::u16string(tier_name->full_name) : std::u16string();
}

}  // namespace subscription_eligibility
