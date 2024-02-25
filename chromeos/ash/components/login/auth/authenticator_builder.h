// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_AUTHENTICATOR_BUILDER_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_AUTHENTICATOR_BUILDER_H_

#include "base/component_export.h"
#include "chromeos/ash/components/login/auth/auth_status_consumer.h"
#include "chromeos/ash/components/login/auth/authenticator.h"
namespace ash {

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH)
    AuthenticatorBuilder {
 public:
  AuthenticatorBuilder() = default;
  AuthenticatorBuilder(const AuthenticatorBuilder&) = delete;
  AuthenticatorBuilder& operator=(const AuthenticatorBuilder&) = delete;
  virtual ~AuthenticatorBuilder() = default;

  virtual scoped_refptr<Authenticator> Create(AuthStatusConsumer* consumer) = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_AUTHENTICATOR_BUILDER_H_
