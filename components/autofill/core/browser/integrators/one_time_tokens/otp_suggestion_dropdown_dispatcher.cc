// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_suggestion_dropdown_dispatcher.h"

#include <utility>
#include <vector>

#include "base/containers/map_util.h"

namespace autofill {

OtpSuggestionDropdownDispatcher::FieldRecord::FieldRecord() = default;
OtpSuggestionDropdownDispatcher::FieldRecord::FieldRecord(const FieldRecord&) =
    default;
OtpSuggestionDropdownDispatcher::FieldRecord&
OtpSuggestionDropdownDispatcher::FieldRecord::operator=(const FieldRecord&) =
    default;
OtpSuggestionDropdownDispatcher::FieldRecord::FieldRecord(
    FieldRecord&&) noexcept = default;
OtpSuggestionDropdownDispatcher::FieldRecord&
OtpSuggestionDropdownDispatcher::FieldRecord::operator=(FieldRecord&&) =
    default;
OtpSuggestionDropdownDispatcher::FieldRecord::~FieldRecord() = default;

OtpSuggestionDropdownDispatcher::OtpSuggestionDropdownDispatcher() = default;
OtpSuggestionDropdownDispatcher::~OtpSuggestionDropdownDispatcher() = default;

bool OtpSuggestionDropdownDispatcher::TriggerProactive(
    const OtpTarget& target,
    const OtpCandidate& candidate,
    bool can_show_on_screen_now,
    TriggerCallback trigger_callback) {
  if (candidate.token.value().empty() || candidate.consumed) {
    return false;
  }

  if (IsSuppressedByDeduplication(target.field, candidate.identity)) {
    return false;
  }

  if (!can_show_on_screen_now) {
    FieldRecord& record = records_by_field_[target.field];
    if (!record.pending_trigger.has_value() ||
        candidate.identity == record.pending_trigger->second ||
        candidate.identity.IsStrictlyNewerThan(
            record.pending_trigger->second)) {
      record.pending_trigger.emplace(target, candidate.identity);
    }
    return false;
  }

  if (!trigger_callback(target)) {
    return false;
  }

  RecordShown(target.field, candidate.identity);
  return true;
}

void OtpSuggestionDropdownDispatcher::RetryPendingTriggers(
    RetryTriggerCallback retry_callback) {
  std::vector<FieldGlobalId> pending_fields;
  for (const auto& [field, record] : records_by_field_) {
    if (record.pending_trigger.has_value()) {
      pending_fields.push_back(field);
    }
  }

  for (FieldGlobalId field : pending_fields) {
    FieldRecord* record = base::FindOrNull(records_by_field_, field);
    if (!record || !record->pending_trigger.has_value()) {
      continue;
    }
    OtpTarget target = std::move(record->pending_trigger->first);
    record->pending_trigger.reset();
    if (!record->last_offered_identity.has_value()) {
      records_by_field_.erase(field);
    }
    retry_callback(target);
  }
}

bool OtpSuggestionDropdownDispatcher::HasPendingTrigger(
    FieldGlobalId field) const {
  const FieldRecord* record = base::FindOrNull(records_by_field_, field);
  return record && record->pending_trigger.has_value();
}

bool OtpSuggestionDropdownDispatcher::IsSuppressedByDeduplication(
    FieldGlobalId field,
    const OtpTokenIdentity& identity) const {
  const FieldRecord* record = base::FindOrNull(records_by_field_, field);
  return record && IsSuppressedByDeduplication(*record, identity);
}

void OtpSuggestionDropdownDispatcher::RecordShown(
    FieldGlobalId field,
    const OtpTokenIdentity& identity) {
  if (identity.value.empty()) {
    return;
  }
  FieldRecord& record = records_by_field_[field];
  if (!record.last_offered_identity.has_value() ||
      identity.IsStrictlyNewerThan(*record.last_offered_identity)) {
    record.last_offered_identity = identity;
  }
  if (record.pending_trigger.has_value() &&
      IsSuppressedByDeduplication(record, record.pending_trigger->second)) {
    record.pending_trigger.reset();
  }
}

void OtpSuggestionDropdownDispatcher::ClearField(FieldGlobalId field) {
  records_by_field_.erase(field);
}

void OtpSuggestionDropdownDispatcher::ClearAll() {
  records_by_field_.clear();
}

bool OtpSuggestionDropdownDispatcher::IsSuppressedByDeduplication(
    const FieldRecord& record,
    const OtpTokenIdentity& identity) const {
  return record.last_offered_identity.has_value() &&
         !identity.IsStrictlyNewerThan(*record.last_offered_identity);
}

}  // namespace autofill
