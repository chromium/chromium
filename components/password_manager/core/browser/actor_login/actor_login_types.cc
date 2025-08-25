// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/actor_login_types.h"

namespace actor_login {

// static
Credential::Id Credential::GenerateCredentialId() {
  static Credential::Id::Generator generator;
  return generator.GenerateNextId();
}

}  // namespace actor_login
