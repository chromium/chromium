// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_PRIMARY_ACCOUNT_CHANGE_EVENT_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_PRIMARY_ACCOUNT_CHANGE_EVENT_H_

#include <optional>
#include <ostream>

#include "build/build_config.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace signin {

class PrimaryAccountChangeEvent {
 public:
  // Used to denote the type of the change event.
  enum class Type {
    // No change.
    kNone = 0,
    // Primary account set or changed.
    kSet = 1,
    // Primary account cleared.
    kCleared = 2
  };

  struct State {
    State();
    State(CoreAccountInfo account_info, ConsentLevel consent_level);
    State(const State& other);
    State& operator=(const State& other);
    ~State();

    friend bool operator==(const State&, const State&) = default;

    CoreAccountInfo primary_account;
    ConsentLevel consent_level = ConsentLevel::kSignin;
  };

  PrimaryAccountChangeEvent();
  PrimaryAccountChangeEvent(
      State previous_state,
      State current_state,
      absl::variant<signin_metrics::AccessPoint, signin_metrics::ProfileSignout>
          event_source);
  ~PrimaryAccountChangeEvent();

  // Returns primary account change event type for the corresponding
  // consent_level. There can be 3 different event types.
  // kNone - No change in primary account for the given consent_level.
  // kSet - A new primary account is set or changed for the given consent_level.
  // kCleared - The primary account set for the consent level is cleared.
  Type GetEventTypeFor(ConsentLevel consent_level) const;

  const State& GetPreviousState() const;
  const State& GetCurrentState() const;

  // Returns the access point used when setting the primary account. Guarateed
  // to have a value when the primary account is set.
  std::optional<signin_metrics::AccessPoint> GetSetPrimaryAccountAccessPoint()
      const;

  // Returns the source that lead to the clearing of the primary account.
  // Guaranteed to have a value when the primary account is cleared.
  std::optional<signin_metrics::ProfileSignout> GetClearPrimaryAccountSource()
      const;

  // Verifies that the states and the event source are valid.
  static bool StatesAndEventSourceAreValid(
      PrimaryAccountChangeEvent::State previous_state,
      PrimaryAccountChangeEvent::State current_state,
      absl::variant<signin_metrics::AccessPoint, signin_metrics::ProfileSignout>
          event_source);

 private:
  State previous_state_, current_state_;
  absl::variant<signin_metrics::AccessPoint, signin_metrics::ProfileSignout>
      event_source_;
};

std::ostream& operator<<(std::ostream& os,
                         const PrimaryAccountChangeEvent::State& state);
std::ostream& operator<<(std::ostream& os,
                         const PrimaryAccountChangeEvent& event);

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
ConvertToJavaPrimaryAccountChangeEvent(
    JNIEnv* env,
    const PrimaryAccountChangeEvent& event_details);
#endif

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_PRIMARY_ACCOUNT_CHANGE_EVENT_H_
