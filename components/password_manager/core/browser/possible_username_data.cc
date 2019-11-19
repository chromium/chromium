// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/possible_username_data.h"

#include "base/strings/string_util.h"

using base::char16;
using base::TimeDelta;

namespace password_manager {

PossibleUsernameData::PossibleUsernameData(std::string signon_realm,
                                           uint32_t renderer_id,
                                           base::string16 value,
                                           base::Time last_change,
                                           int driver_id)
    : signon_realm(std::move(signon_realm)),
      renderer_id(renderer_id),
      value(std::move(value)),
      last_change(last_change),
      driver_id(driver_id) {}
PossibleUsernameData::PossibleUsernameData(const PossibleUsernameData&) =
    default;
PossibleUsernameData::~PossibleUsernameData() = default;

bool IsPossibleUsernameValid(const PossibleUsernameData& possible_username,
                             const std::string& submitted_signon_realm,
                             const base::Time now) {
  if (submitted_signon_realm != possible_username.signon_realm)
    return false;
  // The goal is to avoid false positives in considering which strings might be
  // username. In the initial version of the username first flow it is better to
  // be conservative in that.
  // TODO(https://crbug.com/959776): Reconsider allowing non-ascii symbols in
  // username for the username first flow.
  if (!base::IsStringASCII(possible_username.value))
    return false;
  for (char16 c : possible_username.value) {
    if (base::IsUnicodeWhitespace(c))
      return false;
  }

  if (now - possible_username.last_change >
      kMaxDelayBetweenTypingUsernameAndSubmission) {
    return false;
  }

  return true;
}

}  // namespace password_manager
