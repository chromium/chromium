// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTOR_CORE_SAFETY_LIST_MANAGER_H_
#define COMPONENTS_ACTOR_CORE_SAFETY_LIST_MANAGER_H_

#include <memory>
#include <optional>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/host_indexed_content_settings.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace actor {

class SafetyListManager {
 public:
  // LINT.IfChange(ParseResult)
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ParseResult {
    // The Safety List was successfully parsed.
    kSuccess = 0,
    // The provided string was not valid JSON.
    kInvalidJson = 1,
    // The value associated with the key was not a list.
    kJsonKeyValueNotAList = 2,
    // A value in the list was not a dictionary.
    kJsonListValueNotADictionary = 3,
    // The `to` field was missing or not a string.
    kInvalidToField = 4,
    // The `to` field was not a valid URL pattern.
    kInvalidToUrlPattern = 5,
    // The `from` field was missing or not a string.
    kInvalidFromField = 6,
    // The `from` field was not a valid URL pattern.
    kInvalidFromUrlPattern = 7,
    kMaxValue = kInvalidFromUrlPattern,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:SafetyListParseResult)

  // Verdicts that are supported by the safety lists.
  enum class Decision {
    // No decision was made by the safety lists.
    kNone,
    // The action is allowed by the safety lists.
    kAllow,
    // The action is blocked by the safety lists.
    kBlock,
  };

  ~SafetyListManager();

  SafetyListManager(const SafetyListManager&) = delete;
  SafetyListManager& operator=(const SafetyListManager&) = delete;
  SafetyListManager(SafetyListManager&&) = delete;
  SafetyListManager& operator=(SafetyListManager&&) = delete;

  static SafetyListManager* GetInstance();
  static std::unique_ptr<SafetyListManager> CreateForTesting();

  // Looks up the most specific rule applying to a navigation from `source` to
  // `destination`. If no such rule exists, returns `Decision::kNone`.
  Decision Find(const GURL& source, const GURL& destination) const;

  // ParseSafetyLists parses the input JSON off the main thread and
  // posts the reply back. Callback is invoked when parsing is complete, calling
  // Find() before this callback runs will result in possibly inaccurate
  // results. In practice, this should not matter, since the actor framework
  // calls Find() after startup completes and the user enters a task.
  void ParseSafetyLists(const std::string json_string,
                        base::OnceClosure done_callback);

 private:
  // For singleton pattern.
  friend class base::NoDestructor<SafetyListManager>;
  SafetyListManager();

  struct ParseResultsAndSettings {
    ParseResult allowed_result;
    ParseResult blocked_result;
    std::unique_ptr<content_settings::HostIndexedContentSettings> settings;
  };

  // Private static so it can use ParseResultsAndSettings.
  static ParseResultsAndSettings ParseSafetyListsInternal(
      std::string_view json_string);
  // Private static so it can use ParseSafetyListsInternal.
  static std::unique_ptr<content_settings::HostIndexedContentSettings>
  DoParseSafetyLists(const std::string json_string);
  void OnParsedSafetyLists(
      base::OnceClosure done_callback,
      std::unique_ptr<content_settings::HostIndexedContentSettings>
          new_navigation_settings);

  SEQUENCE_CHECKER(sequence_checker_);

  // Settings for allowing/blocking navigations.
  std::unique_ptr<content_settings::HostIndexedContentSettings>
      navigation_settings_ GUARDED_BY_CONTEXT(sequence_checker_) =
          std::make_unique<content_settings::HostIndexedContentSettings>();
  base::WeakPtrFactory<SafetyListManager> weak_ptr_factory_{this};
};

void ParseSafetyListsForTesting(SafetyListManager* manager,
                                const std::string json);

}  // namespace actor

#endif  // COMPONENTS_ACTOR_CORE_SAFETY_LIST_MANAGER_H_
