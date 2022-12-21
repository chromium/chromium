// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/log_event.h"

#include "base/notreached.h"

namespace autofill {

FillEventId GetNextFillEventId() {
  static FillEventId::Generator fill_event_id_generator;
  return fill_event_id_generator.GenerateNextId();
}

OptionalBoolean& operator|=(OptionalBoolean& a, OptionalBoolean b) {
  if (b == OptionalBoolean::kTrue || a == OptionalBoolean::kUndefined) {
    a = b;
  }

  return a;
}

OptionalBoolean ToOptionalBoolean(bool value) {
  return static_cast<OptionalBoolean>(value);
}

bool OptionalBooleanToBool(OptionalBoolean value) {
  switch (value) {
    case OptionalBoolean::kFalse:
      return false;
    case OptionalBoolean::kTrue:
      return true;
    case OptionalBoolean::kUndefined:
      NOTREACHED();
      return false;
  }

  NOTREACHED();
  return false;
}

bool AreCollapsible(const absl::monostate& event1,
                    const absl::monostate& event2) {
  return true;
}

bool AreCollapsible(const AskForValuesToFillFieldLogEvent& event1,
                    const AskForValuesToFillFieldLogEvent& event2) {
  return event1.has_suggestion == event2.has_suggestion &&
         event1.suggestion_is_shown == event2.suggestion_is_shown;
}

bool AreCollapsible(const TriggerFillFieldLogEvent& event1,
                    const TriggerFillFieldLogEvent& event2) {
  return event1.fill_event_id != event2.fill_event_id;
}

bool AreCollapsible(const FillFieldLogEvent& event1,
                    const FillFieldLogEvent& event2) {
  return event1.fill_event_id != event2.fill_event_id &&
         event1.had_value_before_filling == event2.had_value_before_filling &&
         event1.autofill_skipped_status == event2.autofill_skipped_status &&
         event1.was_autofilled == event2.was_autofilled &&
         event1.had_value_after_filling == event2.had_value_after_filling;
}

bool AreCollapsible(const TypingFieldLogEvent& event1,
                    const TypingFieldLogEvent& event2) {
  return event1.has_value_after_typing == event2.has_value_after_typing;
}

}  // namespace autofill
