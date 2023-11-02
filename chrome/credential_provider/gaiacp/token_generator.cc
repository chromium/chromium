// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/token_generator.h"

#include "base/unguessable_token.h"

namespace credential_provider {

std::string TokenGenerator::GenerateToken() {
  return base::UnguessableToken::Create().ToString();
}

// static
TokenGenerator* TokenGenerator::Get() {
  return *GetInstanceStorage();
}

// static
TokenGenerator** TokenGenerator::GetInstanceStorage() {
  static TokenGenerator instance;
  static TokenGenerator* instance_storage = &instance;
  return &instance_storage;
}

TokenGenerator::TokenGenerator() {}

TokenGenerator::~TokenGenerator() = default;

}  // namespace credential_provider
