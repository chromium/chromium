// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/user_demographics.h"

#include <utility>

#include "base/logging.h"

namespace syncer {

// static
UserDemographicsResult UserDemographicsResult::ForValue(
    UserDemographics value) {
  return UserDemographicsResult(std::move(value),
                                UserDemographicsStatus::kSuccess);
}

// static
UserDemographicsResult UserDemographicsResult::ForStatus(
    UserDemographicsStatus status) {
  DCHECK(status != UserDemographicsStatus::kSuccess);
  return UserDemographicsResult(UserDemographics(), status);
}

bool UserDemographicsResult::IsSuccess() const {
  return status_ == UserDemographicsStatus::kSuccess;
}

UserDemographicsStatus UserDemographicsResult::status() const {
  return status_;
}

const UserDemographics& UserDemographicsResult::value() const {
  return value_;
}

UserDemographicsResult::UserDemographicsResult(UserDemographics value,
                                               UserDemographicsStatus status)
    : value_(std::move(value)), status_(status) {}

}  // namespace syncer
