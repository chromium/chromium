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

// Source features that generate critical actions.
// LINT.IfChange(ActionSource)
enum class ActionSource {
  kUnknown = 0,
  kPasswordManager = 1,
  kActor = 2,
  kMaxValue = kActor,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/critical_actions/histograms.xml:ActionSource)

// Represents a memory row copy of a single record in critical_actions database.
struct CriticalActionEntry {
  CriticalActionEntry();
  CriticalActionEntry(const CriticalActionEntry&);
  CriticalActionEntry(CriticalActionEntry&&) noexcept;
  CriticalActionEntry& operator=(const CriticalActionEntry&);
  CriticalActionEntry& operator=(CriticalActionEntry&&) noexcept;
  ~CriticalActionEntry();

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
  // If set, only records with this conversation_id are returned.
  std::optional<std::string> conversation_id;
  // If set, only records with this actor_task_id are returned.
  std::optional<std::string> actor_task_id;

  // Maximum number of entries to return.
  std::optional<size_t> max_count;
};

}  // namespace critical_actions

#endif  // COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_TYPES_H_
