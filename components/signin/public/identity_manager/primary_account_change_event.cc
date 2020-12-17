// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/primary_account_change_event.h"

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

PrimaryAccountChangeEvent::PrimaryAccountChangeEvent(State previous_state,
                                                     State current_state)
    : previous_state_(previous_state), current_state_(current_state) {}

PrimaryAccountChangeEvent::~PrimaryAccountChangeEvent() = default;

PrimaryAccountChangeEvent::Type PrimaryAccountChangeEvent::GetEventTypeFor(
    ConsentLevel consent_level) const {
  if (previous_state_ == current_state_)
    return Type::kNone;

  if (previous_state_.consent_level == ConsentLevel::kSync) {
    // Cannot change the Sync account without signing out first.
    DCHECK(previous_state_.primary_account == current_state_.primary_account ||
           current_state_.primary_account.IsEmpty());
  }
  if (previous_state_.primary_account == current_state_.primary_account) {
    // Cannot change the consent level for the empty account.
    DCHECK(!previous_state_.primary_account.IsEmpty());
  }

  switch (consent_level) {
    case ConsentLevel::kNotRequired:
      if (previous_state_.primary_account != current_state_.primary_account) {
        return current_state_.primary_account.IsEmpty() ? Type::kCleared
                                                        : Type::kSet;
      }
      return Type::kNone;
    case ConsentLevel::kSync:
      if (previous_state_.consent_level != current_state_.consent_level) {
        return current_state_.consent_level == ConsentLevel::kSync
                   ? Type::kSet
                   : Type::kCleared;
      }
      // Cannot change the Sync account without signing out first.
      DCHECK_EQ(current_state_.consent_level, ConsentLevel::kNotRequired);
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

bool operator==(const PrimaryAccountChangeEvent::State& lhs,
                const PrimaryAccountChangeEvent::State& rhs) {
  return lhs.primary_account == rhs.primary_account &&
         lhs.consent_level == rhs.consent_level;
}

}  // namespace signin