// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/dm_token.h"

#include "base/check.h"

namespace policy {

// static
DMToken DMToken::CreateValidToken(const std::string& value) {
  return DMToken(Status::kValid, value);
}

// static
DMToken DMToken::CreateInvalidToken() {
  return DMToken(Status::kInvalid, "");
}

// static
DMToken DMToken::CreateEmptyToken() {
  return DMToken(Status::kEmpty, "");
}

DMToken::DMToken(Status status, std::string_view value)
    : status_(status), value_(value) {}

const std::string& DMToken::value() const {
  DCHECK(is_valid());
  return value_;
}

bool DMToken::is_valid() const {
  return status_ == Status::kValid;
}

bool DMToken::is_invalid() const {
  return status_ == Status::kInvalid;
}

bool DMToken::is_empty() const {
  return status_ == Status::kEmpty;
}

}  // namespace policy
