// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_TYPES_H_
#define COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_TYPES_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/enum_set.h"
#include "base/time/time.h"

namespace base {
class DictValue;
}  // namespace base

namespace critical_actions {

// Enum defining list of critical categories supported by Critical Action
// History.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.critical_actions
// LINT.IfChange(CriticalActionType)
enum class ActionType {
  kUnknown = 0,
  // Legacy generic form fill, superseded by the form fill groups below.
  // Existing rows keep this type. Don't use it in new code.
  kFormFill = 1,
  kDownload = 2,
  kSettingChange = 3,
  kCredentialAccess = 4,
  kGooglePasswordManager = 5,
  kFederatedLogin = 6,
  kCredentialsOtp = 7,
  kWebMcpTool = 8,
  // Form fill groups mirroring the "Your saved info" settings pages. These
  // supersede the legacy `kFormFill` type:
  // - `kFormFillContactInfo`: address, name, email, phone number.
  // - `kFormFillPayments`: card details.
  kFormFillContactInfo = 9,
  kFormFillPayments = 10,
  kMaxValue = kFormFillPayments,
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

// Attribute filled by a form fill critical action, e.g. an address or card
// details. Stored in CriticalActionEntry::metadata as stable strings under
// "form_fill_types". Append-only; do not renumber.
enum class FormFillType {
  kUnknown = 0,
  // Contact info.
  kAddress = 1,
  kName = 2,
  kEmail = 3,
  kPhone = 4,
  // Payments.
  kCreditCard = 5,
  kMaxValue = kCreditCard,
};

using FormFillTypes = base::
    EnumSet<FormFillType, FormFillType::kAddress, FormFillType::kMaxValue>;

// Returns true for the granular form fill groups, whose records store
// FormFillTypes in their metadata.
bool IsGranularFormFill(ActionType type);

// Returns the form fill group `ActionType` that `type` belongs to.
ActionType GetFormFillGroup(FormFillType type);

// Writes `types` into `metadata_dict` under the "form_fill_types" key.
void SetFormFillTypes(base::DictValue& metadata_dict, FormFillTypes types);

// Identifies which History UI surface requested opening a conversation.
// `kEntryMenu` is the visit-level linkout, which opens the conversation at its
// default position (the "Go to Gemini chat" history entry menu item).
// `kCriticalActionRow` is the per-action linkout, which opens the conversation
// at the action's turn when action-level chat linkouts are enabled.
// LINT.IfChange(OpenConversationSource)
enum class OpenConversationSource {
  kEntryMenu = 0,
  kCriticalActionRow = 1,
  kMaxValue = kCriticalActionRow,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/critical_actions/histograms.xml:OpenConversationSource)

// Maximum allowed byte length for
// `CriticalActionEntry::conversation_anchor_id`. Generous enough for
// agent-agnostic identifiers (e.g., Glic turn IDs, UUIDs, or opaque 3P agent
// tokens) while bounding in-memory caches and SQLite storage.
inline constexpr size_t kMaxConversationAnchorIdLength = 512;

// Represents a memory row copy of a single record in critical_actions database.
struct CriticalActionEntry {
  CriticalActionEntry();
  CriticalActionEntry(const CriticalActionEntry&);
  CriticalActionEntry(CriticalActionEntry&&) noexcept;
  CriticalActionEntry& operator=(const CriticalActionEntry&);
  CriticalActionEntry& operator=(CriticalActionEntry&&) noexcept;
  ~CriticalActionEntry();

  // Returns the user-facing localized label for the critical action. For
  // contact info form fills with attributes, lists them, e.g. "Contact info
  // filled: Address, Name, Email".
  std::string GetLabel() const;

  // Returns the user-facing localized tooltip / description for the action.
  std::string GetTooltip() const;

  // Returns the form fill attributes stored in `metadata`. Empty for legacy
  // rows, malformed JSON and entries without attributes. Unknown strings are
  // ignored.
  FormFillTypes GetFormFillTypes() const;

  // Adds `types` to the "form_fill_types" list in `metadata`, keeping all
  // other keys. Unknown strings are dropped. Changes only this in-memory
  // entry, not the database. Used when merging duplicate entries for display.
  void AddFormFillTypes(FormFillTypes types);

  class Builder;

  std::string critical_action_id;  // Client-generated UUID
  base::Time timestamp;
  int64_t visit_id = 0;         // References History visit
  std::string conversation_id;  // References conversation context
  // Opaque, agent-defined identifier for the position in the conversation
  // where the action occurred (e.g., turn ID for Glic); empty if unknown.
  // Must not exceed kMaxConversationAnchorIdLength bytes.
  std::string conversation_anchor_id;
  std::string actor_task_id;  // References agent task
  ActionType action_type = ActionType::kUnknown;
  ActionSource action_source = ActionSource::kUnknown;
  // Action-specific details in JSON format. Must NOT contain page-identifying
  // data (URLs, origins, hostnames, page titles, or anything derived from
  // them): critical actions deliberately keep no copy of browsing history, and
  // this field is not covered by History's deletion lifecycle. Use `visit_id`
  // to refer to the page the action was performed on.
  std::string metadata;

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
  Builder&& SetConversationId(std::string conversation_id_val) &&;
  Builder&& SetConversationAnchorId(std::string conversation_anchor_id_val) &&;
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
