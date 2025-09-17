// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/actor_login_types.h"

namespace actor_login {

namespace {
Credential::Id GenerateCredentialId() {
  static Credential::Id::Generator generator;
  return generator.GenerateNextId();
}
}  // namespace

Credential::Credential() : id(GenerateCredentialId()) {}

Credential::Credential(const Credential& other) = default;
Credential::Credential(Credential&& other) = default;

Credential& Credential::operator=(const Credential& credential) = default;
Credential& Credential::operator=(Credential&& credential) = default;

Credential::~Credential() = default;
}  // namespace actor_login
