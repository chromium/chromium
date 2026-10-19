// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/actor/core/safety_list_manager.h"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/map_util.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "components/actor/core/actor_features.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/host_indexed_content_settings.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace actor {

namespace {

constexpr std::string_view kNavigationAllowedFieldName = "navigation_allowed";
constexpr std::string_view kNavigationBlockedFieldName = "navigation_blocked";
constexpr std::string_view kPaymentIframeAllowedFieldName =
    "payment_iframe_allowed";

constexpr std::string_view kNavigationAllowedHistogramName =
    "Actor.SafetyListParseResult.NavigationAllowed";
constexpr std::string_view kNavigationBlockedHistogramName =
    "Actor.SafetyListParseResult.NavigationBlocked";
constexpr std::string_view kPaymentIframeAllowedHistogramName =
    "Actor.SafetyListParseResult.PaymentIframeAllowed";

struct SafetyListEntry {
  ContentSettingsPattern source;
  ContentSettingsPattern destination;
};

// Matches `https://<exact-host>:*`. Rules with explicit ports, non-HTTPS
// schemes, or domain wildcards return false and route to `fallback_settings_`.
bool IsSimpleHttpsHostPattern(const ContentSettingsPattern& pattern) {
  return pattern.GetSchemeType() == ContentSettingsPattern::SCHEME_HTTPS &&
         pattern.GetScope() ==
             ContentSettingsPattern::Scope::kWithPortWildcard &&
         !pattern.HasDomainWildcard() && !pattern.GetHost().empty();
}

// Parses a list of entries from a JSON list. Returns the parsed vector on
// success, or a ParseResult on failure. If the result is a ParseResult, the
// enum value is guaranteed to not be `kSuccess`.
base::expected<std::vector<SafetyListEntry>, SafetyListManager::ParseResult>
ParseEntriesFromJson(const base::ListValue& list_data) {
  std::vector<SafetyListEntry> entries;
  entries.reserve(list_data.size());
  for (const auto& navigation : list_data) {
    const base::DictValue* navigation_dict = navigation.GetIfDict();
    if (!navigation_dict) {
      return base::unexpected(
          SafetyListManager::ParseResult::kJsonListValueNotADictionary);
    }

    // Only parse entry if both fields exist.
    const std::string* from = navigation_dict->FindString("from");
    if (!from) {
      return base::unexpected(
          SafetyListManager::ParseResult::kInvalidFromField);
    }
    ContentSettingsPattern source = ContentSettingsPattern::FromString(*from);
    if (!source.IsValid()) {
      return base::unexpected(
          SafetyListManager::ParseResult::kInvalidFromUrlPattern);
    }

    const std::string* to = navigation_dict->FindString("to");
    if (!to) {
      return base::unexpected(SafetyListManager::ParseResult::kInvalidToField);
    }
    ContentSettingsPattern destination =
        ContentSettingsPattern::FromString(*to);
    if (!destination.IsValid()) {
      return base::unexpected(
          SafetyListManager::ParseResult::kInvalidToUrlPattern);
    }
    entries.push_back(
        SafetyListEntry{std::move(source), std::move(destination)});
  }
  return entries;
}

// Parses a list of HTTPS origin strings from a JSON list. Returns the parsed
// flat_set on success, or a ParseResult on failure.
base::expected<base::flat_set<url::Origin>, SafetyListManager::ParseResult>
ParsePaymentIframeAllowedFromList(const base::ListValue& json_list) {
  std::vector<url::Origin> origins;
  origins.reserve(json_list.size());
  for (const auto& item : json_list) {
    const std::string* origin_str = item.GetIfString();
    if (!origin_str) {
      return base::unexpected(
          SafetyListManager::ParseResult::kEntryFormatInvalid);
    }
    GURL url(*origin_str);
    if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme) ||
        url.GetWithEmptyPath() != url) {
      return base::unexpected(
          SafetyListManager::ParseResult::kEntryFormatInvalid);
    }
    url::Origin origin = url::Origin::Create(url);
    if (origin.opaque()) {
      return base::unexpected(
          SafetyListManager::ParseResult::kEntryFormatInvalid);
    }
    origins.push_back(std::move(origin));
  }
  return origins;
}

base::flat_set<std::string> CollectUniqueHosts(
    const std::map<std::string, std::map<std::string, ContentSetting>>&
        host_rules) {
  std::vector<std::string> hosts;
  for (const auto& [src_host, dst_map] : host_rules) {
    hosts.push_back(src_host);
    for (const auto& [dst_host, _] : dst_map) {
      hosts.push_back(dst_host);
    }
  }
  base::flat_set<std::string> unique_hosts(std::move(hosts));
  // The vector may have held many duplicate hosts but it is reused by the
  // flat_set without adjusting the capacity. So we trim the unused capacity
  // before returning.
  unique_hosts.shrink_to_fit();
  return unique_hosts;
}

// Projects a `std::map` into a `base::flat_map`. `key_projection` must preserve
// the strict weak ordering of the keys in `map`.
template <
    typename IK,
    typename IV,
    typename KeyProj = std::identity,
    typename ValueProj = std::identity,
    typename OK = std::remove_cvref_t<std::invoke_result_t<KeyProj, const IK&>>,
    typename OV =
        std::remove_cvref_t<std::invoke_result_t<ValueProj, const IV&>>>
base::flat_map<OK, OV> ToFlatMapWithoutSorting(
    const std::map<IK, IV>& map,
    KeyProj key_projection = {},
    ValueProj value_projection = {}) {
  return base::flat_map<OK, OV>(
      base::sorted_unique,
      base::ToVector<std::pair<OK, OV>>(map, [&](const auto& pair) {
        const auto& [k, v] = pair;
        return std::pair(std::invoke(key_projection, k),
                         std::invoke(value_projection, v));
      }));
}

}  // namespace

SafetyListManager::SettingsStore::SettingsStore(
    const std::map<std::string, std::map<std::string, ContentSetting>>&
        https_host_rules,
    content_settings::HostIndexedContentSettings fallback_settings,
    base::flat_set<url::Origin> payment_iframe_allowed_origins)
    : owned_hosts_(CollectUniqueHosts(https_host_rules)),
      https_host_scoped_settings_(ToFlatMapWithoutSorting(
          https_host_rules,
          [&](const std::string& host) { return GetViewFromPool(host); },
          [&](const std::map<std::string, ContentSetting>& inner_map) {
            return ToFlatMapWithoutSorting(
                inner_map,
                [&](const std::string& host) { return GetViewFromPool(host); });
          })),
      fallback_settings_(std::move(fallback_settings)),
      payment_iframe_allowed_origins_(
          std::move(payment_iframe_allowed_origins)) {}
SafetyListManager::SettingsStore::~SettingsStore() = default;

class SafetyListManager::SettingsStore::Builder {
 public:
  bool empty() const {
    return https_host_rules_.empty() && fallback_settings_.empty() &&
           payment_iframe_allowed_origins_.empty();
  }

  std::unique_ptr<SettingsStore> Build() && {
    return std::make_unique<SettingsStore>(
        https_host_rules_, std::move(fallback_settings_),
        std::move(payment_iframe_allowed_origins_));
  }

  ParseResult ParseListFromDict(const base::DictValue& json_dict,
                                std::string_view field_name,
                                ContentSetting setting) {
    const base::Value* value = json_dict.Find(field_name);
    if (!value) {
      return ParseResult::kSuccess;
    }
    const base::ListValue* list = value->GetIfList();
    if (!list) {
      return ParseResult::kJsonKeyValueNotAList;
    }
    ASSIGN_OR_RETURN(std::vector<SafetyListEntry> result,
                     ParseEntriesFromJson(*list));
    AddEntries(result, setting);
    return ParseResult::kSuccess;
  }

  SafetyListManager::ParseResult ParsePaymentIframeAllowedFromDict(
      const base::DictValue& json_dict) {
    const base::Value* value = json_dict.Find(kPaymentIframeAllowedFieldName);
    if (!value) {
      return ParseResult::kSuccess;
    }
    const base::ListValue* list = value->GetIfList();
    if (!list) {
      return ParseResult::kJsonKeyValueNotAList;
    }
    ASSIGN_OR_RETURN(payment_iframe_allowed_origins_,
                     ParsePaymentIframeAllowedFromList(*list));
    return ParseResult::kSuccess;
  }

 private:
  void AddEntries(base::span<const SafetyListEntry> entries,
                  ContentSetting setting) {
    for (const auto& entry : entries) {
      if (IsSimpleHttpsHostPattern(entry.source) &&
          IsSimpleHttpsHostPattern(entry.destination)) {
        https_host_rules_[entry.source.GetHost()][entry.destination.GetHost()] =
            setting;
      } else {
        fallback_settings_.SetValue(
            entry.source, entry.destination,
            content_settings::ContentSettingToValue(setting), {});
      }
    }
  }

  std::map<std::string, std::map<std::string, ContentSetting>>
      https_host_rules_;
  content_settings::HostIndexedContentSettings fallback_settings_;
  base::flat_set<url::Origin> payment_iframe_allowed_origins_;
};

SafetyListManager::Decision SafetyListManager::SettingsStore::Find(
    const GURL& source,
    const GURL& destination) const {
  std::optional<ContentSetting> found_setting;
  if (source.SchemeIs(url::kHttpsScheme) &&
      destination.SchemeIs(url::kHttpsScheme)) {
    if (const DestSettingsMap* dst_map =
            base::FindOrNull(https_host_scoped_settings_, source.host())) {
      if (const ContentSetting* setting =
              base::FindOrNull(*dst_map, destination.host())) {
        found_setting = *setting;
      }
    }
  }
  if (!found_setting.has_value()) {
    const content_settings::RuleEntry* rule_entry =
        fallback_settings_.Find(source, destination);
    if (!rule_entry) {
      return Decision::kNone;
    }
    found_setting =
        content_settings::ParseContentSettingValue(rule_entry->second.value)
            .value();
  }

  switch (found_setting.value()) {
    case CONTENT_SETTING_ALLOW:
      return Decision::kAllow;
    case CONTENT_SETTING_BLOCK:
      return kGlicEnforceComponentUpdaterBlockListEntries.Get()
                 ? Decision::kBlock
                 : Decision::kNone;
    case CONTENT_SETTING_DEFAULT:
    case CONTENT_SETTING_ASK:
    case CONTENT_SETTING_SESSION_ONLY:
    case CONTENT_SETTING_NUM_SETTINGS:
      NOTREACHED();
  }
  NOTREACHED();
}

bool SafetyListManager::SettingsStore::IsPaymentIframeOriginAllowed(
    const url::Origin& origin) const {
  return payment_iframe_allowed_origins_.contains(origin);
}

std::string_view SafetyListManager::SettingsStore::GetViewFromPool(
    std::string_view host) const {
  auto it = owned_hosts_.find(host);
  CHECK(it != owned_hosts_.end() && *it == host);
  return std::string_view(*it);
}

// static
SafetyListManager* SafetyListManager::GetInstance() {
  static base::NoDestructor<SafetyListManager> instance;
  return instance.get();
}

// static
std::unique_ptr<SafetyListManager> SafetyListManager::CreateForTesting() {
  return base::WrapUnique(new SafetyListManager());
}

void SafetyListManager::Find(const GURL& source,
                             const GURL& destination,
                             FindCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (load_safety_lists_closure_ || IsParseInProgress()) {
    pending_finds_.emplace_back(
        base::BindOnce(&SafetyListManager::Find, weak_ptr_factory_.GetWeakPtr(),
                       source, destination, std::move(callback)));

    MaybeStartParse();
  } else {
    std::move(callback).Run(FindSync(source, destination));
  }
}

void SafetyListManager::IsPaymentIframeOriginAllowed(
    const url::Origin& origin,
    PaymentIframeOriginAllowedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (load_safety_lists_closure_ || IsParseInProgress()) {
    pending_finds_.emplace_back(base::BindOnce(
        &SafetyListManager::IsPaymentIframeOriginAllowed,
        weak_ptr_factory_.GetWeakPtr(), origin, std::move(callback)));

    MaybeStartParse();
  } else {
    std::move(callback).Run(IsPaymentIframeOriginAllowedSync(origin));
  }
}

SafetyListManager::Decision SafetyListManager::FindSync(
    const GURL& source,
    const GURL& destination) const {
  if (!store_) {
    return Decision::kNone;
  }
  return store_->Find(source, destination);
}

bool SafetyListManager::IsPaymentIframeOriginAllowedSync(
    const url::Origin& origin) const {
  if (!store_) {
    return false;
  }
  return store_->IsPaymentIframeOriginAllowed(origin);
}

SafetyListManager::SafetyListManager() = default;
SafetyListManager::~SafetyListManager() = default;

// static
SafetyListManager::ParseResultsAndSettings
SafetyListManager::ParseSafetyListsInternal(std::string_view json_string) {
  std::optional<base::Value> json =
      base::JSONReader::Read(json_string, base::JSON_PARSE_RFC);
  if (!json.has_value()) {
    return {SafetyListManager::ParseResult::kInvalidJson,
            SafetyListManager::ParseResult::kInvalidJson,
            SafetyListManager::ParseResult::kInvalidJson, nullptr};
  }

  base::DictValue* json_dict = json->GetIfDict();
  if (!json_dict) {
    return {ParseResult::kInvalidJson, ParseResult::kInvalidJson,
            ParseResult::kInvalidJson, nullptr};
  }

  SettingsStore::Builder builder;

  ParseResult allowed_result =
      builder.ParseListFromDict(*json_dict, kNavigationAllowedFieldName,
                                ContentSetting::CONTENT_SETTING_ALLOW);
  ParseResult blocked_result =
      builder.ParseListFromDict(*json_dict, kNavigationBlockedFieldName,
                                ContentSetting::CONTENT_SETTING_BLOCK);
  ParseResult payment_iframe_allowed_result =
      builder.ParsePaymentIframeAllowedFromDict(*json_dict);

  std::unique_ptr<SettingsStore> store = nullptr;
  if (!builder.empty()) {
    store = std::move(builder).Build();
  }

  return {allowed_result, blocked_result, payment_iframe_allowed_result,
          std::move(store)};
}

// static
std::unique_ptr<SafetyListManager::SettingsStore>
SafetyListManager::DoParseSafetyLists(LoadSafetyListsClosure closure) {
  base::AssertBlockingAllowed();
  std::optional<std::string> json_string = std::move(closure).Run();
  if (!json_string.has_value()) {
    return {};
  }
  SafetyListManager::ParseResultsAndSettings result =
      ParseSafetyListsInternal(*json_string);
  base::UmaHistogramEnumeration(kNavigationAllowedHistogramName,
                                result.allowed_result);
  base::UmaHistogramEnumeration(kNavigationBlockedHistogramName,
                                result.blocked_result);
  base::UmaHistogramEnumeration(kPaymentIframeAllowedHistogramName,
                                result.payment_iframe_allowed_result);
  return std::move(result.store);
}

void SafetyListManager::OnParsedSafetyLists(
    std::unique_ptr<SafetyListManager::SettingsStore> new_store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Need to explicitly invalidate weak pointers here so that
  // `IsParseInProgress()` returns false, before running any pending `Find`
  // operations.
  parse_weak_ptr_factory_.InvalidateWeakPtrs();

  if (new_store) {
    store_ = std::move(new_store);
  }
  PendingFinds pending = std::exchange(pending_finds_, {});
  for (base::OnceClosure& closure : pending) {
    std::move(closure).Run();
  }
}

bool SafetyListManager::IsParseInProgress() const {
  return parse_weak_ptr_factory_.HasWeakPtrs();
}

void SafetyListManager::SetLoadSafetyListsClosure(
    LoadSafetyListsClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(closure);
  load_safety_lists_closure_ = std::move(closure);

  if (pending_finds_.empty()) {
    return;
  }
  MaybeStartParse();
}

void SafetyListManager::MaybeStartParse() {
  if (!load_safety_lists_closure_) {
    return;
  }
  parse_weak_ptr_factory_.InvalidateWeakPtrs();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&SafetyListManager::DoParseSafetyLists,
                     std::move(load_safety_lists_closure_)),
      base::BindOnce(&SafetyListManager::OnParsedSafetyLists,
                     parse_weak_ptr_factory_.GetWeakPtr()));
}

void SetSafetyListsForTesting(SafetyListManager* manager, std::string json) {
  CHECK(manager);
  manager->SetLoadSafetyListsClosure(base::BindOnce(
      [](std::string json) -> std::optional<std::string> { return json; },
      std::move(json)));
}

}  // namespace actor
