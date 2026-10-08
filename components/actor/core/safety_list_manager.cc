// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/actor/core/safety_list_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
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

// Returns a span of elements from an expected vector. If the `expected` is an
// error, then the result span is empty.
//
// This returns a span that references `elts`, so the span is only valid as long
// as `elts` is valid.
base::span<const SafetyListEntry> SpanOverExpected(
    const base::expected<std::vector<SafetyListEntry>,
                         SafetyListManager::ParseResult>& elts) {
  return elts.has_value() ? *elts : base::span<const SafetyListEntry>();
}

void SetAll(base::span<const SafetyListEntry> entries,
            ContentSetting setting,
            content_settings::HostIndexedContentSettings& indexed_settings) {
  const base::Value setting_value =
      content_settings::ContentSettingToValue(setting);
  for (const auto& entry : entries) {
    indexed_settings.SetValue(entry.source, entry.destination,
                              setting_value.Clone(), {});
  }
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
  return base::flat_set<url::Origin>(std::move(origins));
}

base::expected<base::flat_set<url::Origin>, SafetyListManager::ParseResult>
ParsePaymentIframeAllowedFromDict(const base::DictValue& json_dict) {
  const base::Value* value = json_dict.Find(kPaymentIframeAllowedFieldName);
  if (!value) {
    return base::flat_set<url::Origin>();
  }
  const base::ListValue* list = value->GetIfList();
  if (!list) {
    return base::unexpected(
        SafetyListManager::ParseResult::kJsonKeyValueNotAList);
  }
  return ParsePaymentIframeAllowedFromList(*list);
}

}  // namespace

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
  const content_settings::RuleEntry* rule_entry =
      navigation_settings_->Find(source, destination);

  if (!rule_entry) {
    return Decision::kNone;
  }
  ContentSetting setting =
      content_settings::ParseContentSettingValue(rule_entry->second.value)
          .value();
  switch (setting) {
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

bool SafetyListManager::IsPaymentIframeOriginAllowedSync(
    const url::Origin& origin) const {
  return payment_iframe_allowed_origins_.contains(origin);
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
            SafetyListManager::ParseResult::kInvalidJson,
            {}};
  }

  base::DictValue* json_dict = json->GetIfDict();
  if (!json_dict) {
    return {ParseResult::kInvalidJson,
            ParseResult::kInvalidJson,
            ParseResult::kInvalidJson,
            {}};
  }

  auto parse_one_list = [&json_dict](std::string_view field_name)
      -> base::expected<std::vector<SafetyListEntry>,
                        SafetyListManager::ParseResult> {
    if (const base::Value* value = json_dict->Find(field_name)) {
      if (const base::ListValue* list = value->GetIfList()) {
        return ParseEntriesFromJson(*list);
      }
      return base::unexpected(ParseResult::kJsonKeyValueNotAList);
    }
    return std::vector<SafetyListEntry>();
  };

  base::expected<std::vector<SafetyListEntry>, SafetyListManager::ParseResult>
      allowed_result = parse_one_list(kNavigationAllowedFieldName);
  base::expected<std::vector<SafetyListEntry>, SafetyListManager::ParseResult>
      blocked_result = parse_one_list(kNavigationBlockedFieldName);
  std::unique_ptr<content_settings::HostIndexedContentSettings>
      navigation_settings;
  if (allowed_result.has_value() || blocked_result.has_value()) {
    navigation_settings =
        std::make_unique<content_settings::HostIndexedContentSettings>();
    SetAll(SpanOverExpected(allowed_result),
           ContentSetting::CONTENT_SETTING_ALLOW, *navigation_settings);
    SetAll(SpanOverExpected(blocked_result),
           ContentSetting::CONTENT_SETTING_BLOCK, *navigation_settings);
  }

  base::expected<base::flat_set<url::Origin>, SafetyListManager::ParseResult>
      payment_iframe_allowed_result =
          ParsePaymentIframeAllowedFromDict(*json_dict);

  return {
      allowed_result.error_or(ParseResult::kSuccess),
      blocked_result.error_or(ParseResult::kSuccess),
      payment_iframe_allowed_result.error_or(ParseResult::kSuccess),
      {std::move(navigation_settings),
       base::OptionalFromExpected(std::move(payment_iframe_allowed_result))},
  };
}

// static
SafetyListManager::ParsedSafetyLists SafetyListManager::DoParseSafetyLists(
    LoadSafetyListsClosure closure) {
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
  return std::move(result.parsed_lists);
}

void SafetyListManager::OnParsedSafetyLists(ParsedSafetyLists parsed_lists) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Need to explicitly invalidate weak pointers here so that
  // `IsParseInProgress()` returns false, before running any pending `Find`
  // operations.
  parse_weak_ptr_factory_.InvalidateWeakPtrs();

  if (parsed_lists.settings) {
    navigation_settings_ = std::move(parsed_lists.settings);
  }
  if (parsed_lists.payment_iframe_allowed_origins.has_value()) {
    payment_iframe_allowed_origins_ =
        *std::move(parsed_lists.payment_iframe_allowed_origins);
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
