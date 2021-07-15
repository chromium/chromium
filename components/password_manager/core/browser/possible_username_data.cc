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
    const std::u16string& field_name,
    const std::u16string& value,
    base::Time last_change,
    int driver_id)
    : signon_realm(std::move(signon_realm)),
      renderer_id(renderer_id),
      field_name(field_name),
      value(value),
      last_change(last_change),
      driver_id(driver_id) {}
PossibleUsernameData::PossibleUsernameData(const PossibleUsernameData&) =
    default;
PossibleUsernameData::~PossibleUsernameData() = default;

bool IsPossibleUsernameStale(const PossibleUsernameData& possible_username) {
  return base::Time::Now() - possible_username.last_change >
         kPossibleUsernameExpirationTimeout;
}

}  // namespace password_manager
