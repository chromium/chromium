// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_SUGGESTION_DROPDOWN_DISPATCHER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_SUGGESTION_DROPDOWN_DISPATCHER_H_

// Declares `OtpSuggestionDropdownDispatcher`, the tab-scoped helper owned by
// `OtpCoordinator` for deduplicating proactive OTP triggers and deferring them
// while the tab cannot display a suggestion popup.

#include <optional>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/function_ref.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_types.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

// Tab-scoped dispatcher owned by `OtpCoordinator` that triggers proactive OTP
// suggestions through the `OtpSuggestionProvider` owning the form, defers
// triggers while the tab is hidden or its window is unfocused, and enforces
// identity-based deduplication so the same OTP is not repeatedly triggered.
class OtpSuggestionDropdownDispatcher {
 public:
  // Synchronously invoked by `TriggerProactive()` to request that the
  // form-owning frame's `OtpSuggestionProvider` trigger the suggestion popup
  // in the renderer. Returns whether the renderer trigger was dispatched;
  // deduplication state is only recorded when this returns true.
  using TriggerCallback = base::FunctionRef<bool(const OtpTarget&)>;

  // Synchronously invoked by `RetryPendingTriggers()` for each previously
  // deferred target when the tab becomes active.
  using RetryTriggerCallback = base::FunctionRef<void(const OtpTarget&)>;

  OtpSuggestionDropdownDispatcher();
  OtpSuggestionDropdownDispatcher(const OtpSuggestionDropdownDispatcher&) =
      delete;
  OtpSuggestionDropdownDispatcher& operator=(
      const OtpSuggestionDropdownDispatcher&) = delete;
  ~OtpSuggestionDropdownDispatcher();

  // Triggers a proactive suggestion for `target` with `candidate` if a
  // strictly newer OTP has arrived.
  // If `can_show_on_screen_now` is false (e.g. tab hidden or window
  // unfocused), records `target` as pending to be retried when the tab becomes
  // active.
  // Returns true if `trigger_callback` was invoked and succeeded.
  bool TriggerProactive(const OtpTarget& target,
                        const OtpCandidate& candidate,
                        bool can_show_on_screen_now,
                        TriggerCallback trigger_callback);

  // Extracts all pending targets and invokes `retry_callback` for each one
  // when the tab becomes visible and its window is focused.
  void RetryPendingTriggers(RetryTriggerCallback retry_callback);

  // Returns true if `field` currently has a deferred proactive trigger waiting
  // for the tab/window to become active.
  bool HasPendingTrigger(FieldGlobalId field) const;

  // Returns true if `identity` is suppressed on `field` because the same or a
  // newer OTP has already been offered or shown on `field`, i.e. `identity` is
  // not strictly newer than the last offered OTP.
  bool IsSuppressedByDeduplication(FieldGlobalId field,
                                   const OtpTokenIdentity& identity) const;

  // Records that `identity` was displayed in an OTP suggestion popup for
  // `field` (`OnSuggestionsShown()` with an OTP suggestion type) so proactive
  // triggers do not re-trigger the same OTP.
  void RecordShown(FieldGlobalId field, const OtpTokenIdentity& identity);

  // Clears pending and deduplication state for `field` when the field is
  // removed or navigated away.
  void ClearField(FieldGlobalId field);

  // Clears all pending and deduplication state.
  void ClearAll();

 private:
  struct FieldRecord {
    FieldRecord();
    FieldRecord(const FieldRecord&);
    FieldRecord& operator=(const FieldRecord&);
    FieldRecord(FieldRecord&&) noexcept;
    FieldRecord& operator=(FieldRecord&&);
    ~FieldRecord();

    std::optional<OtpTokenIdentity> last_offered_identity;
    std::optional<std::pair<OtpTarget, OtpTokenIdentity>> pending_trigger;
  };

  bool IsSuppressedByDeduplication(const FieldRecord& record,
                                   const OtpTokenIdentity& identity) const;

  base::flat_map<FieldGlobalId, FieldRecord> records_by_field_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_SUGGESTION_DROPDOWN_DISPATCHER_H_
