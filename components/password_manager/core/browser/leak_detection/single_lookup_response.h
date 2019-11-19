// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_SINGLE_LOOKUP_RESPONSE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_SINGLE_LOOKUP_RESPONSE_H_

#include <string>
#include <vector>

namespace password_manager {

// This class encapsulates the data required to determine whether a given
// credential was leaked. It is a more convenient data representation of the
// protobuf used for network communication.
struct SingleLookupResponse {
  SingleLookupResponse();
  SingleLookupResponse(const SingleLookupResponse& other);
  SingleLookupResponse& operator=(const SingleLookupResponse& other);
  SingleLookupResponse(SingleLookupResponse&& other);
  SingleLookupResponse& operator=(SingleLookupResponse&& other);
  ~SingleLookupResponse();

  std::vector<std::string> encrypted_leak_match_prefixes;
  std::string reencrypted_lookup_hash;
};

bool operator==(const SingleLookupResponse& lhs,
                const SingleLookupResponse& rhs);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_SINGLE_LOOKUP_RESPONSE_H_
