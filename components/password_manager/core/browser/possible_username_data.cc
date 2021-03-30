// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/possible_username_data.h"

#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/strings/string_piece.h"
#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"

using base::TimeDelta;

namespace password_manager {

PossibleUsernameData::PossibleUsernameData(
    std::string signon_realm,
    autofill::FieldRendererId renderer_id,
    std::u16string value,
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

bool IsPossibleUsernameValid(
    const PossibleUsernameData& possible_username,
    const std::string& submitted_signon_realm,
    const std::vector<std::u16string>& possible_usernames) {
  if (submitted_signon_realm != possible_username.signon_realm)
    return false;

  // The goal is to avoid false positives in considering which strings might be
  // username. In the initial version of the username first flow it is better to
  // be conservative in that. This check only allows usernames that match
  // existing usernames after canonicalization.
  std::u16string (*Canonicalize)(base::StringPiece16) = &CanonicalizeUsername;
  if (!base::Contains(possible_usernames, Canonicalize(possible_username.value),
                      Canonicalize)) {
    return false;
  }

  return base::Time::Now() - possible_username.last_change <=
         kMaxDelayBetweenTypingUsernameAndSubmission;
}

}  // namespace password_manager
