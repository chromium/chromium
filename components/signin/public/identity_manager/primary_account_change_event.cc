// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/primary_account_change_event.h"

#include "base/check_op.h"
#include "build/build_config.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_utils.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/signin/public/android/jni_headers/PrimaryAccountChangeEvent_jni.h"
#endif

namespace signin {

PrimaryAccountChangeEvent::State::State() = default;

PrimaryAccountChangeEvent::State::State(const State& other) = default;

PrimaryAccountChangeEvent::State::State(CoreAccountInfo account_info,
                                        ConsentLevel consent_level)
    : primary_account(account_info), consent_level(consent_level) {}

PrimaryAccountChangeEvent::State::~State() = default;

PrimaryAccountChangeEvent::State& PrimaryAccountChangeEvent::State::operator=(
    const State& other) = default;

PrimaryAccountChangeEvent::PrimaryAccountChangeEvent() = default;

PrimaryAccountChangeEvent::PrimaryAccountChangeEvent(
    State previous_state,
    State current_state,
    absl::variant<signin_metrics::AccessPoint, signin_metrics::ProfileSignout>
        event_source)
    : previous_state_(previous_state),
      current_state_(current_state),
      event_source_(event_source) {
  CHECK(StatesAndEventSourceAreValid(previous_state_, current_state_,
                                     event_source_));
}

PrimaryAccountChangeEvent::~PrimaryAccountChangeEvent() = default;

PrimaryAccountChangeEvent::Type PrimaryAccountChangeEvent::GetEventTypeFor(
    ConsentLevel consent_level) const {
  if (previous_state_ == current_state_)
    return Type::kNone;

  // Cannot change the Sync account without clearing the primary account first.
  // TODO(crbug.com/40067058): Delete this when ConsentLevel::kSync is
  //     deleted. See ConsentLevel::kSync documentation for details.
  DCHECK(previous_state_.consent_level != ConsentLevel::kSync ||
         previous_state_.primary_account == current_state_.primary_account ||
         current_state_.primary_account.IsEmpty());

  // Cannot change the consent level for the empty account.
  DCHECK(previous_state_.primary_account != current_state_.primary_account ||
         !previous_state_.primary_account.IsEmpty());

  switch (consent_level) {
    case ConsentLevel::kSignin:
      if (previous_state_.primary_account != current_state_.primary_account) {
        return current_state_.primary_account.IsEmpty() ? Type::kCleared
                                                        : Type::kSet;
      }
      return Type::kNone;
    case ConsentLevel::kSync:
      // TODO(crbug.com/40067058): Delete this when ConsentLevel::kSync is
      //     deleted. See ConsentLevel::kSync documentation for details.
      if (previous_state_.consent_level != current_state_.consent_level) {
        return current_state_.consent_level == ConsentLevel::kSync
                   ? Type::kSet
                   : Type::kCleared;
      }
      // Cannot change the Sync account without clearing the primary account
      // first.
      DCHECK_EQ(current_state_.consent_level, ConsentLevel::kSignin);
      return Type::kNone;
  }
}

const PrimaryAccountChangeEvent::State&
PrimaryAccountChangeEvent::GetCurrentState() const {
  return current_state_;
}

const PrimaryAccountChangeEvent::State&
PrimaryAccountChangeEvent::GetPreviousState() const {
  return previous_state_;
}

std::optional<signin_metrics::AccessPoint>
PrimaryAccountChangeEvent::GetSetPrimaryAccountAccessPoint() const {
  if (absl::holds_alternative<signin_metrics::AccessPoint>(event_source_)) {
    return std::optional<signin_metrics::AccessPoint>(
        absl::get<signin_metrics::AccessPoint>(event_source_));
  }
  return std::nullopt;
}

std::optional<signin_metrics::ProfileSignout>
PrimaryAccountChangeEvent::GetClearPrimaryAccountSource() const {
  if (absl::holds_alternative<signin_metrics::ProfileSignout>(event_source_)) {
    return std::optional<signin_metrics::ProfileSignout>(
        absl::get<signin_metrics::ProfileSignout>(event_source_));
  }
  return std::nullopt;
}

bool PrimaryAccountChangeEvent::StatesAndEventSourceAreValid(
    PrimaryAccountChangeEvent::State previous_state,
    PrimaryAccountChangeEvent::State current_state,
    absl::variant<signin_metrics::AccessPoint, signin_metrics::ProfileSignout>
        event_source) {
  // The states cannot have an empty primary account and the consent level
  // kSync.
  if (previous_state.primary_account.IsEmpty() &&
      previous_state.consent_level == ConsentLevel::kSync) {
    return false;
  }
  if (current_state.primary_account.IsEmpty() &&
      current_state.consent_level == ConsentLevel::kSync) {
    return false;
  }

  // If the previous state's primary account is empty and the current state's is
  // not, the event source should be an access point.
  if (previous_state.primary_account.IsEmpty() &&
      !current_state.primary_account.IsEmpty() &&
      !absl::holds_alternative<signin_metrics::AccessPoint>(event_source)) {
    return false;
  }

  // If the previous state's primary account is not empty and the current
  // state's is empty, the event source should be a profile sign out.
  if (!previous_state.primary_account.IsEmpty() &&
      current_state.primary_account.IsEmpty() &&
      !absl::holds_alternative<signin_metrics::ProfileSignout>(event_source)) {
    return false;
  }

  // If the previous state's consent level is kSignin and the current state's is
  // kSync, the event source should be an access point.
  if (previous_state.consent_level == ConsentLevel::kSignin &&
      current_state.consent_level == ConsentLevel::kSync &&
      !absl::holds_alternative<signin_metrics::AccessPoint>(event_source)) {
    return false;
  }

  // If the previous state's consent level is kSync and the current state's is
  // kSignin, the event source should be a profile sign out.
  if (previous_state.consent_level == ConsentLevel::kSync &&
      current_state.consent_level == ConsentLevel::kSignin &&
      !absl::holds_alternative<signin_metrics::ProfileSignout>(event_source)) {
    return false;
  }

  // If the primary account changes and the states' consent level stay the same,
  // the event source should be an access point.
  if (!current_state.primary_account.IsEmpty() &&
      previous_state.consent_level == current_state.consent_level &&
      previous_state.primary_account != current_state.primary_account &&
      !absl::holds_alternative<signin_metrics::AccessPoint>(event_source)) {
    return false;
  }

  return true;
}

std::ostream& operator<<(std::ostream& os,
                         const PrimaryAccountChangeEvent::State& state) {
  os << "{ primary_account: " << state.primary_account.account_id << ", "
     << "consent_level:"
     << (state.primary_account.IsEmpty()                ? ""
         : state.consent_level == ConsentLevel::kSignin ? " Signin"
                                                        : " Sync")
     << " }";
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const PrimaryAccountChangeEvent& event) {
  os << "{ previous_state: " << event.GetPreviousState() << ", "
     << "current_state: " << event.GetCurrentState() << " }";
  return os;
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
ConvertToJavaPrimaryAccountChangeEvent(
    JNIEnv* env,
    const PrimaryAccountChangeEvent& event_details) {
  PrimaryAccountChangeEvent::Type event_type_not_required =
      event_details.GetEventTypeFor(ConsentLevel::kSignin);
  // TODO(crbug.com/40067058): Delete this when ConsentLevel::kSync is
  //     deleted. See ConsentLevel::kSync documentation for details.
  PrimaryAccountChangeEvent::Type event_type_sync =
      event_details.GetEventTypeFor(ConsentLevel::kSync);
  // Should not fire events if there is no change in primary accounts for any
  // consent level.
  DCHECK(event_type_not_required != PrimaryAccountChangeEvent::Type::kNone ||
         event_type_sync != PrimaryAccountChangeEvent::Type::kNone);
  return Java_PrimaryAccountChangeEvent_Constructor(
      env, jint(event_type_not_required), jint(event_type_sync));
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace signin
