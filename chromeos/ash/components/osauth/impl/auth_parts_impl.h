// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_PARTS_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_PARTS_IMPL_H_

#include <memory>

#include "base/component_export.h"
#include "chromeos/ash/components/osauth/public/auth_parts.h"

namespace ash {

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) AuthPartsImpl
    : public AuthParts {
 public:
  // Creates an empty instance to be used in unit tests.
  static std::unique_ptr<AuthPartsImpl> CreateTestInstance();

  AuthPartsImpl();
  ~AuthPartsImpl() override;

  // AuthParts implementation:
  AuthSessionStorage* GetAuthSessionStorage() override;

 private:
  friend class AuthParts;
  void CreateDefaultComponents();

  std::unique_ptr<AuthSessionStorage> session_storage_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_PARTS_IMPL_H_
