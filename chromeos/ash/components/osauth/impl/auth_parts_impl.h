// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_PARTS_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_PARTS_IMPL_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "chromeos/ash/components/osauth/public/auth_parts.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

class AuthFactorEngineFactory;
class AuthHub;
class AuthSessionStorage;

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) AuthPartsImpl
    : public AuthParts {
 public:
  // Creates an empty instance to be used in unit tests.
  static std::unique_ptr<AuthPartsImpl> CreateTestInstance();

  AuthPartsImpl();
  ~AuthPartsImpl() override;

  // AuthParts implementation:
  AuthSessionStorage* GetAuthSessionStorage() override;
  AuthHub* GetAuthHub() override;
  void RegisterEngineFactory(
      std::unique_ptr<AuthFactorEngineFactory> factory) override;
  const std::vector<std::unique_ptr<AuthFactorEngineFactory>>&
  GetEngineFactories() override;

  // Test-related setters:
  void SetAuthHub(std::unique_ptr<AuthHub> auth_hub);

 private:
  friend class AuthParts;
  void CreateDefaultComponents();

  std::unique_ptr<AuthHub> auth_hub_;
  std::unique_ptr<AuthSessionStorage> session_storage_;

  std::vector<std::unique_ptr<AuthFactorEngineFactory>> engine_factories_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_PARTS_IMPL_H_
