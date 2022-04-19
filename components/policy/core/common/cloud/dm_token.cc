// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/dm_token.h"

namespace policy {

// static
DMToken DMToken::CreateValidTokenForTesting(const std::string& value) {
  return DMToken(Status::kValid, value);
}

// static
DMToken DMToken::CreateInvalidTokenForTesting() {
  return DMToken(Status::kInvalid, "");
}

// static
DMToken DMToken::CreateEmptyTokenForTesting() {
  return DMToken(Status::kEmpty, "");
}

DMToken::DMToken() : DMToken(Status::kEmpty, "") {}

DMToken::DMToken(Status status, const base::StringPiece value)
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
