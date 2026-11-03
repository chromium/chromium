// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subscription_eligibility/ai_subscription_tier_names.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/map_util.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
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

// Both names of a tier in `kAiTierOverrides`. Owns the strings that the
// `AiTierName` returned by `FindTierName()` points to.
struct OverriddenTierName {
  std::u16string short_name;
  std::u16string full_name;

  AiTierName AsAiTierName() const {
    return {.short_name = short_name, .full_name = full_name};
  }
};

// Valid content of `kAiTierOverrides`. A tier can be in both `tier_names` and
// `excluded_tiers`: `GetAiSubscriptionTierDisplayInfo()` checks exclusion
// first.
struct TierOverrides {
  base::flat_map<int, OverriddenTierName> tier_names;
  base::flat_set<int> excluded_tiers;
};

// Returns the tier written as `key`, or std::nullopt if `key` is not the
// canonical decimal representation of an integer. Rejecting e.g. "02" or "+2"
// ensures that two distinct keys can't name the same tier (literal duplicate
// keys are collapsed by the JSON reader: the last one wins). Tiers <= 0 are
// accepted here and ignored by `GetAiSubscriptionTierDisplayInfo()`.
std::optional<int> ParseTierKey(std::string_view key) {
  int tier = 0;
  if (!base::StringToInt(key, &tier) || base::NumberToString(tier) != key) {
    return std::nullopt;
  }
  return tier;
}

// Parses `kAiTierOverrides`. Returns std::nullopt if `json` is not valid, see
// the header.
std::optional<TierOverrides> ParseTierOverrides(std::string_view json) {
  // Keys of `kAiTierOverrides`.
  static constexpr char kTierNamesKey[] = "tier_names";
  static constexpr char kShortNameKey[] = "short_name";
  static constexpr char kFullNameKey[] = "full_name";
  static constexpr char kExcludedTiersKey[] = "excluded_tiers";

  std::optional<base::DictValue> dict =
      base::JSONReader::ReadDict(json, base::JSON_PARSE_RFC);
  if (!dict) {
    return std::nullopt;
  }

  TierOverrides overrides;
  if (const base::Value* value = dict->Find(kTierNamesKey)) {
    const base::DictValue* tier_names = value->GetIfDict();
    if (!tier_names) {
      return std::nullopt;
    }
    for (const auto [key, names_value] : *tier_names) {
      std::optional<int> tier = ParseTierKey(key);
      const base::DictValue* names = names_value.GetIfDict();
      if (!tier || !names) {
        return std::nullopt;
      }
      const std::string* short_name = names->FindString(kShortNameKey);
      const std::string* full_name = names->FindString(kFullNameKey);
      if (!short_name || !full_name) {
        return std::nullopt;
      }
      overrides.tier_names.emplace(
          *tier,
          OverriddenTierName{.short_name = base::UTF8ToUTF16(*short_name),
                             .full_name = base::UTF8ToUTF16(*full_name)});
    }
  }

  if (const base::Value* value = dict->Find(kExcludedTiersKey)) {
    const base::ListValue* excluded_tiers = value->GetIfList();
    if (!excluded_tiers) {
      return std::nullopt;
    }
    for (const base::Value& entry : *excluded_tiers) {
      std::optional<int> tier = entry.GetIfInt();
      if (!tier) {
        return std::nullopt;
      }
      overrides.excluded_tiers.insert(*tier);
    }
  }

  return overrides;
}

// Returns the parsed `kAiTierOverrides`, or std::nullopt if
// `kAiSubscriptionTierOverrides` is disabled or the value is not valid.
std::optional<TierOverrides> ParseOverridesFromFeatureParam() {
  if (!base::FeatureList::IsEnabled(kAiSubscriptionTierOverrides)) {
    return std::nullopt;
  }
  return ParseTierOverrides(kAiTierOverrides.Get());
}

// Returns the parsed `kAiTierOverrides`. It can't change while Chrome runs, so
// it is only parsed once (or again by `ResetAiTierOverridesForTesting()`).
std::optional<TierOverrides>& GetCachedOverrides() {
  static base::NoDestructor<std::optional<TierOverrides>> overrides(
      ParseOverridesFromFeatureParam());
  return *overrides;
}

// Returns the names of `tier`: from `overrides` if it lists the tier, else
// from `kAiTierNames`. Returns std::nullopt for a tier unknown to both. The
// returned names point to `overrides` or to `kAiTierNames`.
std::optional<AiTierName> FindTierName(
    int tier,
    const std::optional<TierOverrides>& overrides) {
  if (overrides) {
    if (const OverriddenTierName* name =
            base::FindOrNull(overrides->tier_names, tier)) {
      return name->AsAiTierName();
    }
  }
  if (const AiTierName* name = base::FindOrNull(kAiTierNames, tier)) {
    return *name;
  }
  return std::nullopt;
}

// Resolves the presentation of `tier` given the parsed overrides, see
// `GetAiSubscriptionTierDisplayInfo()`.
AiSubscriptionTierDisplayInfo ResolveDisplayInfo(
    int tier,
    const std::optional<TierOverrides>& overrides) {
  if (tier <= 0) {
    return {};
  }
  if (overrides && overrides->excluded_tiers.contains(tier)) {
    return {};
  }
  const std::optional<AiTierName> name = FindTierName(tier, overrides);
  if (!name) {
    return {.show_decoration = true};
  }
  return {.show_decoration = true,
          .badge_label = std::u16string(name->short_name),
          .full_membership_label = std::u16string(name->full_name)};
}

}  // namespace

BASE_FEATURE(kAiSubscriptionTierOverrides, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kAiTierOverrides{
    &kAiSubscriptionTierOverrides, "ai_tier_overrides", "{}"};

AiSubscriptionTierDisplayInfo GetAiSubscriptionTierDisplayInfo(int tier) {
  return ResolveDisplayInfo(tier, GetCachedOverrides());
}

void ResetAiTierOverridesForTesting() {
  GetCachedOverrides() = ParseOverridesFromFeatureParam();
}

}  // namespace subscription_eligibility
