// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/actor/core/safety_list_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_restrictions.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/actor/core/actor_features.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/host_indexed_content_settings.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "url/gurl.h"

namespace actor {

namespace {

constexpr std::string_view kNavigationAllowedFieldName = "navigation_allowed";
constexpr std::string_view kNavigationBlockedFieldName = "navigation_blocked";

constexpr std::string_view kNavigationAllowedHistogramName =
    "Actor.SafetyListParseResult.NavigationAllowed";
constexpr std::string_view kNavigationBlockedHistogramName =
    "Actor.SafetyListParseResult.NavigationBlocked";

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

  std::visit(
      absl::Overload{
          [&](NotYetParsed& not_yet_parsed) {
            if (not_yet_parsed.load_safety_lists_closure ||
                IsParseInProgress()) {
              not_yet_parsed.pending_finds.emplace_back(base::BindOnce(
                  &SafetyListManager::Find, weak_ptr_factory_.GetWeakPtr(),
                  source, destination, std::move(callback)));

              not_yet_parsed.MaybeStartParse(parse_weak_ptr_factory_);
            } else {
              std::move(callback).Run(Decision::kNone);
            }
          },
          [&](Parsed& parsed) {
            std::move(callback).Run(parsed.Find(source, destination));
          },
      },
      state_);
}

SafetyListManager::Decision SafetyListManager::Parsed::Find(
    const GURL& source,
    const GURL& destination) const {
  const content_settings::RuleEntry* rule_entry =
      navigation_settings->Find(source, destination);

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

SafetyListManager::SafetyListManager() = default;
SafetyListManager::~SafetyListManager() = default;

// static
SafetyListManager::ParseResultsAndSettings
SafetyListManager::ParseSafetyListsInternal(std::string_view json_string) {
  std::optional<base::Value> json =
      base::JSONReader::Read(json_string, base::JSON_PARSE_RFC);
  if (!json.has_value()) {
    return {SafetyListManager::ParseResult::kInvalidJson,
            SafetyListManager::ParseResult::kInvalidJson, nullptr};
  }

  base::DictValue* json_dict = json->GetIfDict();
  if (!json_dict) {
    return {ParseResult::kInvalidJson, ParseResult::kInvalidJson, nullptr};
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

  return {allowed_result.error_or(ParseResult::kSuccess),
          blocked_result.error_or(ParseResult::kSuccess),
          std::move(navigation_settings)};
}

// static
std::unique_ptr<content_settings::HostIndexedContentSettings>
SafetyListManager::DoParseSafetyLists(LoadSafetyListsClosure closure) {
  base::AssertBlockingAllowed();
  std::optional<std::string> json_string = std::move(closure).Run();
  if (!json_string.has_value()) {
    return nullptr;
  }
  SafetyListManager::ParseResultsAndSettings result =
      ParseSafetyListsInternal(*json_string);
  base::UmaHistogramEnumeration(kNavigationAllowedHistogramName,
                                result.allowed_result);
  base::UmaHistogramEnumeration(kNavigationBlockedHistogramName,
                                result.blocked_result);
  return std::move(result.settings);
}

void SafetyListManager::OnParsedSafetyLists(
    std::unique_ptr<content_settings::HostIndexedContentSettings>
        new_navigation_settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Need to explicitly invalidate weak pointers here so that
  // `IsParseInProgress()` returns false, before running any pending `Find`
  // operations.
  parse_weak_ptr_factory_.InvalidateWeakPtrs();

  // This function is only reachable in the `NotYetParsed` state because this is
  // called when parses complete, at most one parse is in progress at one time,
  // every parse starts from the `NotYetParsed` state, and this is the only
  // function that sets `state_` to the `Parsed` state. So std::get is safe
  // here.

  PendingFinds pending =
      std::move(std::get<NotYetParsed>(state_).pending_finds);
  if (new_navigation_settings) {
    state_ = Parsed{std::move(new_navigation_settings)};
  }
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
  if (std::holds_alternative<Parsed>(state_)) {
    state_ = NotYetParsed{std::move(closure), {}};
    return;
  }

  NotYetParsed& not_yet_parsed = std::get<NotYetParsed>(state_);
  not_yet_parsed.load_safety_lists_closure = std::move(closure);
  if (!not_yet_parsed.pending_finds.empty()) {
    not_yet_parsed.MaybeStartParse(parse_weak_ptr_factory_);
  }
}

void SafetyListManager::NotYetParsed::MaybeStartParse(
    base::WeakPtrFactory<SafetyListManager>& parse_weak_ptr_factory) {
  if (!load_safety_lists_closure) {
    return;
  }
  parse_weak_ptr_factory.InvalidateWeakPtrs();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&SafetyListManager::DoParseSafetyLists,
                     std::move(load_safety_lists_closure)),
      base::BindOnce(&SafetyListManager::OnParsedSafetyLists,
                     parse_weak_ptr_factory.GetWeakPtr()));
}

void SetSafetyListsForTesting(SafetyListManager* manager, std::string json) {
  CHECK(manager);
  manager->SetLoadSafetyListsClosure(base::BindOnce(
      [](std::string json) -> std::optional<std::string> { return json; },
      std::move(json)));
}

}  // namespace actor
