// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subscription_eligibility/ai_subscription_tier_names.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/branding_buildflags.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "components/internal/subscription_eligibility/ai_subscription_tier_names_branded.h"
#else
#include "components/subscription_eligibility/ai_subscription_tier_names_specs.h"
#endif

namespace subscription_eligibility {

namespace {

// An entry of `kAiTierShortNames` or `kAiTierFullNames`: {tier, name}.
using TierName = decltype(kAiTierShortNames)::value_type;

// Both maps must list the same tiers.
static_assert(std::ranges::equal(kAiTierShortNames,
                                 kAiTierFullNames,
                                 {},
                                 &TierName::first,
                                 &TierName::first));

// Returns the name of `tier` in `names`, or an empty string if `tier` is not
// listed.
template <size_t N>
std::u16string GetName(
    const base::fixed_flat_map<int, std::u16string_view, N>& names,
    int tier) {
  auto it = names.find(tier);
  if (it == names.end()) {
    return std::u16string();
  }
  return std::u16string(it->second);
}

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
  return GetName(kAiTierShortNames, tier);
}

std::u16string GetAiSubscriptionTierFullName(int tier) {
  // An overridden short name would not match the built-in full name: let the
  // caller use a generic string instead.
  if (FindShortNameOverride(tier)) {
    return std::u16string();
  }
  return GetName(kAiTierFullNames, tier);
}

}  // namespace subscription_eligibility
