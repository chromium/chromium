// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_TYPES_H_
#define COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_TYPES_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "url/gurl.h"

namespace critical_actions {

// Enum defining list of critical categories supported by Critical Action
// History.
// LINT.IfChange(CriticalActionType)
enum class ActionType {
  kUnknown = 0,
  kFormFill = 1,
  kDownload = 2,
  kSettingChange = 3,
  kCredentialAccess = 4,
  kGooglePasswordManager = 5,
  kFederatedLogin = 6,
  kCredentialsOtp = 7,
  kMaxValue = kCredentialsOtp,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/critical_actions/enums.xml:CriticalActionType)

// Source features that generate critical actions.
// LINT.IfChange(ActionSource)
enum class ActionSource {
  kUnknown = 0,
  kPasswordManager = 1,
  kActor = 2,
  kAutofill = 3,
  kMaxValue = kAutofill,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/critical_actions/histograms.xml:ActionSource)

// Result of attempting to open a conversation associated with a critical
// action.
// LINT.IfChange(OpenConversationResult)
enum class OpenConversationResult {
  kSuccess = 0,
  kErrorInvalidActionEntry = 1,
  kErrorInternal = 2,
  kMaxValue = kErrorInternal,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/critical_actions/enums.xml:OpenConversationResult)

// Represents a memory row copy of a single record in critical_actions database.
struct CriticalActionEntry {
  CriticalActionEntry();
  CriticalActionEntry(const CriticalActionEntry&);
  CriticalActionEntry(CriticalActionEntry&&) noexcept;
  CriticalActionEntry& operator=(const CriticalActionEntry&);
  CriticalActionEntry& operator=(CriticalActionEntry&&) noexcept;
  ~CriticalActionEntry();

  // Returns the user-facing localized label for the critical action.
  std::string GetLabel() const;

  // Returns the user-facing localized tooltip / description for the action.
  std::string GetTooltip() const;

  class Builder;

  std::string critical_action_id;  // Client-generated UUID
  base::Time timestamp;
  int64_t visit_id = 0;         // References History visit
  std::string conversation_id;  // References conversation context
  std::string actor_task_id;    // References agent task
  ActionType action_type = ActionType::kUnknown;
  ActionSource action_source = ActionSource::kUnknown;
  GURL url;
  std::string metadata;  // Action-specific details in JSON format

  bool operator==(const CriticalActionEntry& other) const = default;
};

class CriticalActionEntry::Builder {
 public:
  Builder();
  ~Builder();
  Builder(const Builder&) = delete;
  Builder& operator=(const Builder&) = delete;
  Builder(Builder&&) noexcept;
  Builder& operator=(Builder&&) noexcept;

  Builder&& SetCriticalActionId(std::string critical_action_id_val) &&;
  Builder&& SetTimestamp(base::Time timestamp_val) &&;
  Builder&& SetActionType(ActionType action_type_val) &&;
  Builder&& SetActionSource(ActionSource action_source_val) &&;
  Builder&& SetUrl(GURL url_val) &&;
  Builder&& SetConversationId(std::string conversation_id_val) &&;
  Builder&& SetActorTaskId(std::string actor_task_id_val) &&;
  Builder&& SetMetadata(std::string metadata_val) &&;
  Builder&& SetVisitId(int64_t visit_id_val) &&;

  CriticalActionEntry Build() &&;

 private:
  CriticalActionEntry entry_;
};

// Options for querying critical action history.
struct CriticalActionQueryOptions {
  CriticalActionQueryOptions();
  CriticalActionQueryOptions(const CriticalActionQueryOptions&);
  CriticalActionQueryOptions(CriticalActionQueryOptions&&) noexcept;
  CriticalActionQueryOptions& operator=(const CriticalActionQueryOptions&);
  CriticalActionQueryOptions& operator=(CriticalActionQueryOptions&&) noexcept;
  ~CriticalActionQueryOptions();

  // If set, only records at or after this time are returned.
  std::optional<base::Time> begin_time;
  // If set, only records before this time are returned.
  std::optional<base::Time> end_time;

  // If set, only records with these action types are returned.
  std::vector<ActionType> action_types;
  // If set, only records with these visit IDs are returned.
  std::vector<int64_t> visit_ids;
  // If set, only records with this conversation_id are returned.
  std::optional<std::string> conversation_id;
  // If set, only records with this actor_task_id are returned.
  std::optional<std::string> actor_task_id;

  // Maximum number of entries to return.
  std::optional<size_t> max_count;
};

}  // namespace critical_actions

#endif  // COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_TYPES_H_
