// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_prefs_registry.h"

#include <algorithm>
#include <array>
#include <string_view>

#include "base/json/values_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/signin/public/base/signin_prefs_keys.h"

namespace {

using PrefDescriptor = SigninPrefsRegistry::PrefDescriptor;

template <size_t N>
consteval std::array<std::string_view, SigninPrefsRegistry::kMaxParentDepth>
ToParentStorage(const std::array<std::string_view, N>& parents) {
  static_assert(N <= SigninPrefsRegistry::kMaxParentDepth,
                "Raise SigninPrefsRegistry::kMaxParentDepth");
  std::array<std::string_view, SigninPrefsRegistry::kMaxParentDepth> storage =
      {};
  for (size_t i = 0; i < N; ++i) {
    storage[i] = parents[i];
  }
  return storage;
}

// Registry of all active per-account preferences managed by `SigninPrefs`.
// LINT.IfChange(SigninPrefsKeys)
constexpr PrefDescriptor kRegisteredSigninPrefs[] = {
    {.key = signin::internal::kChromeLastSignoutTime,
     .type = base::Value::Type::STRING,
     .is_timestamp = true},
    {.key = signin::internal::kChromeSigninInterceptionUserChoice,
     .type = base::Value::Type::INTEGER},
    {.key = signin::internal::kChromeSigninInterceptionLastBubbleDeclineTime,
     .type = base::Value::Type::STRING,
     .is_timestamp = true},
    {.key = signin::internal::kChromeSigninInterceptionRepromptCount,
     .type = base::Value::Type::INTEGER},
    {.key = signin::internal::kChromeSigninInterceptionDismissCount,
     .type = base::Value::Type::INTEGER},
    {.key = signin::internal::kCrossDevicePromoPrefs,
     .type = base::Value::Type::DICT},
    {.key = signin::internal::kCrossDevicePromoHistoryDictKey,
     .parent_keys_storage =
         ToParentStorage(signin::internal::kCrossDevicePromoRootParents),
     .type = base::Value::Type::DICT},
    {.key = signin::internal::kCrossDevicePromoShownCountKey,
     .parent_keys_storage =
         ToParentStorage(signin::internal::kCrossDeviceHistoryPromoParents),
     .type = base::Value::Type::INTEGER},
    {.key = signin::internal::kCrossDevicePromoLastDismissedTimeKey,
     .parent_keys_storage =
         ToParentStorage(signin::internal::kCrossDeviceHistoryPromoParents),
     .type = base::Value::Type::STRING,
     .is_timestamp = true},
    {.key = signin::internal::kCrossDevicePromoShownAfterDismissalKey,
     .parent_keys_storage =
         ToParentStorage(signin::internal::kCrossDeviceHistoryPromoParents),
     .type = base::Value::Type::BOOLEAN},
    {.key = signin::internal::kPasswordSignInPromoShownCount,
     .type = base::Value::Type::INTEGER},
    {.key = signin::internal::kAddressSignInPromoShownCount,
     .type = base::Value::Type::INTEGER},
    {.key = signin::internal::kBookmarkSignInPromoShownCount,
     .type = base::Value::Type::INTEGER},
    {.key = signin::internal::kAutofillSignInPromoDismissCount,
     .type = base::Value::Type::INTEGER},
    {.key = signin::internal::kBookmarkManagerSignInPromoShownCount,
     .type = base::Value::Type::INTEGER},
    {.key = signin::internal::kBookmarkManagerSignInPromoDismissCount,
     .type = base::Value::Type::INTEGER},
    {.key = signin::internal::kSearchAIModeSignInPromoShownCount,
     .type = base::Value::Type::INTEGER},
    {.key = signin::internal::kSearchAIModeSignInPromoDismissCount,
     .type = base::Value::Type::INTEGER},
    {.key = signin::internal::kSearchAIModeSignInPromoLastImpressionTime,
     .type = base::Value::Type::STRING,
     .is_timestamp = true},
    {.key = signin::internal::kExtensionsExplicitBrowserSigninEnabled,
     .type = base::Value::Type::BOOLEAN},
    {.key = signin::internal::kBookmarksExplicitBrowserSigninEnabled,
     .type = base::Value::Type::BOOLEAN},
    {.key = signin::internal::kBookmarkBatchUploadPromoDismissCount,
     .type = base::Value::Type::INTEGER},
    {.key = signin::internal::kBookmarkBatchUploadPromoLastDismissTime,
     .type = base::Value::Type::STRING,
     .is_timestamp = true},
    {.key = signin::internal::kPolicyDisclaimerLastRegistrationFailureTime,
     .type = base::Value::Type::STRING,
     .is_timestamp = true},
    {.key = signin::internal::kAvatarButtonPromoCountDictionary,
     .type = base::Value::Type::DICT},
    {.key = signin::internal::kAvatarButtonHistorySyncPromoShownCount,
     .parent_keys_storage =
         ToParentStorage(signin::internal::kAvatarButtonPromoParents),
     .type = base::Value::Type::INTEGER},
    {.key = signin::internal::kAvatarButtonHistorySyncPromoUsedCount,
     .parent_keys_storage =
         ToParentStorage(signin::internal::kAvatarButtonPromoParents),
     .type = base::Value::Type::INTEGER},
    {.key = signin::internal::kAvatarButtonBatchUploadPromoShownCount,
     .parent_keys_storage =
         ToParentStorage(signin::internal::kAvatarButtonPromoParents),
     .type = base::Value::Type::INTEGER},
    {.key = signin::internal::kAvatarButtonBatchUploadPromoUsedCount,
     .parent_keys_storage =
         ToParentStorage(signin::internal::kAvatarButtonPromoParents),
     .type = base::Value::Type::INTEGER},
    {.key = signin::internal::kAvatarButtonBatchUploadBookmarkPromoShownCount,
     .parent_keys_storage =
         ToParentStorage(signin::internal::kAvatarButtonPromoParents),
     .type = base::Value::Type::INTEGER},
    {.key = signin::internal::kAvatarButtonBatchUploadBookmarkPromoUsedCount,
     .parent_keys_storage =
         ToParentStorage(signin::internal::kAvatarButtonPromoParents),
     .type = base::Value::Type::INTEGER},
    {.key = signin::internal::
         kAvatarButtonBatchUploadWindows10DepreciationPromoShownCount,
     .parent_keys_storage =
         ToParentStorage(signin::internal::kAvatarButtonPromoParents),
     .type = base::Value::Type::INTEGER},
    {.key = signin::internal::
         kAvatarButtonBatchUploadWindows10DepreciationPromoUsedCount,
     .parent_keys_storage =
         ToParentStorage(signin::internal::kAvatarButtonPromoParents),
     .type = base::Value::Type::INTEGER},
    {.key = signin::internal::kAvatarButtonSigninPromoShownCount,
     .parent_keys_storage =
         ToParentStorage(signin::internal::kAvatarButtonPromoParents),
     .type = base::Value::Type::INTEGER},
    {.key = signin::internal::kAvatarButtonSigninPromoUsedCount,
     .parent_keys_storage =
         ToParentStorage(signin::internal::kAvatarButtonPromoParents),
     .type = base::Value::Type::INTEGER},
    {.key = signin::internal::kAvatarButtonSigninPromoLastShownTime,
     .parent_keys_storage =
         ToParentStorage(signin::internal::kAvatarButtonPromoParents),
     .type = base::Value::Type::STRING,
     .is_timestamp = true},
    {.key = signin::internal::kAccountMetricsId,
     .type = base::Value::Type::INTEGER},
    {.key = signin::internal::kAccountMetricsIdIsCapped,
     .type = base::Value::Type::BOOLEAN},
    {.key = signin::internal::kBatchUploadLastUploadRemainingLocalDataCount,
     .type = base::Value::Type::INTEGER},
    {.key = signin::internal::kHistoryPageHistorySyncPromoShownCount,
     .type = base::Value::Type::INTEGER},
    {.key = signin::internal::kHistoryPageHistorySyncPromoShownAfterDismissal,
     .type = base::Value::Type::BOOLEAN},
    {.key =
         signin::internal::kHistoryPageHistorySyncPromoLastDismissedTimestamp,
     .type = base::Value::Type::STRING,
     .is_timestamp = true},
};
// LINT.ThenChange(//components/signin/public/base/signin_prefs_keys.h:SigninPrefsKeys)

constexpr bool SamePath(base::span<const std::string_view> parents_a,
                        std::string_view key_a,
                        base::span<const std::string_view> parents_b,
                        std::string_view key_b) {
  return key_a == key_b && std::ranges::equal(parents_a, parents_b);
}

constexpr const PrefDescriptor* FindPrefDescriptor(
    base::span<const PrefDescriptor> prefs,
    base::span<const std::string_view> parents,
    std::string_view key) {
  for (const auto& pref : prefs) {
    if (SamePath(pref.parent_keys(), pref.key, parents, key)) {
      return &pref;
    }
  }
  return nullptr;
}

constexpr bool HasNonEmptyUniqueKeys(base::span<const PrefDescriptor> prefs) {
  for (size_t i = 0; i < prefs.size(); ++i) {
    if (prefs[i].key.empty()) {
      return false;
    }
    for (size_t j = i + 1; j < prefs.size(); ++j) {
      if (SamePath(prefs[i].parent_keys(), prefs[i].key, prefs[j].parent_keys(),
                   prefs[j].key)) {
        return false;
      }
    }
  }
  return true;
}

constexpr bool TimestampsAreStrings(base::span<const PrefDescriptor> prefs) {
  for (const auto& pref : prefs) {
    if (pref.is_timestamp && pref.type != base::Value::Type::STRING) {
      return false;
    }
  }
  return true;
}

constexpr bool ParentsAreRegisteredDicts(
    base::span<const PrefDescriptor> prefs) {
  for (const auto& pref : prefs) {
    base::span<const std::string_view> parents = pref.parent_keys();
    for (size_t depth = 0; depth < parents.size(); ++depth) {
      const PrefDescriptor* parent_def =
          FindPrefDescriptor(prefs, parents.first(depth), parents[depth]);
      if (!parent_def || parent_def->type != base::Value::Type::DICT) {
        return false;
      }
    }
  }
  return true;
}

constexpr bool DeprecatedPrefsAreUnregistered(
    base::span<const PrefDescriptor> prefs,
    base::span<const std::string_view> deprecated_keys) {
  for (std::string_view deprecated : deprecated_keys) {
    for (const auto& pref : prefs) {
      if (pref.key == deprecated) {
        return false;
      }
    }
  }
  return true;
}

// Ensures that no registered key contains a dot prefix matching a registered
// dictionary key at the same level (e.g. a key "foo.bar" alongside a dict
// "foo"). This prevents ambiguous path resolutions for
// chrome://signin-prefs-internals.
constexpr bool NoDottedKeyShadowsDict(base::span<const PrefDescriptor> prefs) {
  for (const auto& dict : prefs) {
    if (dict.type != base::Value::Type::DICT) {
      continue;
    }
    for (const auto& other : prefs) {
      if (&dict == &other ||
          !std::ranges::equal(dict.parent_keys(), other.parent_keys())) {
        continue;
      }
      if (other.key.size() > dict.key.size() &&
          other.key.starts_with(dict.key) &&
          other.key[dict.key.size()] == '.') {
        return false;
      }
    }
  }
  return true;
}

constexpr bool TypesAreSet(base::span<const PrefDescriptor> prefs) {
  for (const auto& pref : prefs) {
    if (pref.type != base::Value::Type::BOOLEAN &&
        pref.type != base::Value::Type::INTEGER &&
        pref.type != base::Value::Type::STRING &&
        pref.type != base::Value::Type::DICT) {
      return false;
    }
  }
  return true;
}

constexpr bool ParentKeysStorageHasNoGaps(
    base::span<const PrefDescriptor> prefs) {
  for (const auto& pref : prefs) {
    bool seen_empty = false;
    for (std::string_view parent : pref.parent_keys_storage) {
      if (parent.empty()) {
        seen_empty = true;
      } else if (seen_empty) {
        return false;
      }
    }
  }
  return true;
}

static_assert(HasNonEmptyUniqueKeys(kRegisteredSigninPrefs));
static_assert(TypesAreSet(kRegisteredSigninPrefs));
static_assert(ParentKeysStorageHasNoGaps(kRegisteredSigninPrefs));
static_assert(TimestampsAreStrings(kRegisteredSigninPrefs));
static_assert(ParentsAreRegisteredDicts(kRegisteredSigninPrefs));
static_assert(
    DeprecatedPrefsAreUnregistered(kRegisteredSigninPrefs,
                                   signin::internal::kDeprecatedSigninPrefs));
static_assert(NoDottedKeyShadowsDict(kRegisteredSigninPrefs));

}  // namespace

// static
base::span<const PrefDescriptor> SigninPrefsRegistry::GetAll(PassKey) {
  return kRegisteredSigninPrefs;
}

// static
const PrefDescriptor* SigninPrefsRegistry::Find(
    PassKey,
    base::span<const std::string_view> parents,
    std::string_view key) {
  return FindPrefDescriptor(kRegisteredSigninPrefs, parents, key);
}

// static
bool SigninPrefsRegistry::IsValidValue(
    PassKey,
    base::span<const std::string_view> parents,
    std::string_view key,
    const base::Value& value) {
  // Intermediate container dictionaries cannot be set as leaf values.
  if (value.is_dict()) {
    return false;
  }
  const PrefDescriptor* pref_def =
      FindPrefDescriptor(kRegisteredSigninPrefs, parents, key);
  if (!pref_def || pref_def->type != value.type()) {
    return false;
  }
  if (pref_def->is_timestamp && !base::ValueToTime(&value).has_value()) {
    return false;
  }
  return true;
}
