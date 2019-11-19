// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_DM_TOKEN_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_DM_TOKEN_H_

#include <string>

#include "base/strings/string_piece.h"
#include "components/policy/policy_export.h"

namespace policy {

// Represents a DM token with a status, which can be:
// Valid:
//    A valid token to be used to make requests. Its value cannot be empty or
//    equal to |kInvalidTokenValue|.
// Invalid:
//    The token explicitly marks this browser as unenrolled. The browser
//    should not sync policies or try to get a new DM token if it is set to
//    this value.
// Empty:
//    The token is empty. The browser will try to get a valid token if an
//    enrollment token is present.
class POLICY_EXPORT DMToken {
 public:
  enum class Status { kValid, kInvalid, kEmpty };

  static DMToken CreateValidTokenForTesting(const std::string& value);
  static DMToken CreateInvalidTokenForTesting();
  static DMToken CreateEmptyTokenForTesting();

  DMToken();
  DMToken(Status status, const base::StringPiece value);

  DMToken(const DMToken& other) = default;
  DMToken(DMToken&& other) = default;

  DMToken& operator=(const DMToken& other) = default;
  DMToken& operator=(DMToken&& other) = default;
  ~DMToken() = default;

  // Returns the DM token string value. Should only be called on a valid token.
  const std::string& value() const;

  // Helpers that check the status of the token. Theses states are mutually
  // exclusive, so one function returning true implies all other return false.
  bool is_valid() const;
  bool is_invalid() const;
  bool is_empty() const;

 private:
  Status status_;
  std::string value_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_DM_TOKEN_H_
