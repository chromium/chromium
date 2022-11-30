// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_TOKEN_GENERATOR_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_TOKEN_GENERATOR_H_

#include <string>

namespace credential_provider {

class TokenGenerator {
 public:
  // Returns the storage used for the instance pointer.
  static TokenGenerator** GetInstanceStorage();

  static TokenGenerator* Get();

  // Generates a 128-bit token from a cryptographically strong random source.
  virtual std::string GenerateToken();

 protected:
  TokenGenerator();
  virtual ~TokenGenerator();
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_TOKEN_GENERATOR_H_
