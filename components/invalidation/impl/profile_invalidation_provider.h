// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_PROFILE_INVALIDATION_PROVIDER_H_
#define COMPONENTS_INVALIDATION_IMPL_PROFILE_INVALIDATION_PROVIDER_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/compiler_specific.h"
#include "components/invalidation/impl/profile_identity_provider.h"
#include "components/keyed_service/core/keyed_service.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace invalidation {

class InvalidationService;

// A KeyedService that owns an InvalidationService.
class ProfileInvalidationProvider : public KeyedService {
 public:
  using CustomSenderInvalidationServiceFactory =
      base::RepeatingCallback<std::unique_ptr<InvalidationService>(
          const std::string&)>;

  ProfileInvalidationProvider(
      std::unique_ptr<IdentityProvider> identity_provider,
      CustomSenderInvalidationServiceFactory
          custom_sender_invalidation_service_factory = {});
  ProfileInvalidationProvider(const ProfileInvalidationProvider& other) =
      delete;
  ProfileInvalidationProvider& operator=(
      const ProfileInvalidationProvider& other) = delete;
  ~ProfileInvalidationProvider() override;

  // Returns the InvalidationService specific to |sender_id|.
  InvalidationService* GetInvalidationServiceForCustomSender(
      const std::string& sender_id);

  IdentityProvider* GetIdentityProvider();

  // KeyedService:
  void Shutdown() override;

  // Register prefs to be used by per-Profile instances of this class which
  // store invalidation state in Profile prefs.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  std::unique_ptr<IdentityProvider> identity_provider_;

  CustomSenderInvalidationServiceFactory
      custom_sender_invalidation_service_factory_;
  std::unordered_map<std::string, std::unique_ptr<InvalidationService>>
      custom_sender_invalidation_services_;
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_PROFILE_INVALIDATION_PROVIDER_H_
