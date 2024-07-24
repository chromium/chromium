// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_PROFILE_INVALIDATION_PROVIDER_H_
#define COMPONENTS_INVALIDATION_PROFILE_INVALIDATION_PROVIDER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>

#include "base/compiler_specific.h"
#include "components/invalidation/impl/profile_identity_provider.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/keyed_service/core/keyed_service.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace invalidation {

// A KeyedService that owns InvalidationService instances (legacy) and
// InvalidationListener instances for sender ids (Pantheon project ids).
class ProfileInvalidationProvider : public KeyedService {
 public:
  using InvalidationServiceOrListenerFactory =
      base::RepeatingCallback<std::variant<
          std::unique_ptr<InvalidationService>,
          std::unique_ptr<InvalidationListener>>(std::string /*sender_id*/,
                                                 std::string /*project_id*/,
                                                 std::string /*log_prefix*/)>;

  ProfileInvalidationProvider(
      std::unique_ptr<IdentityProvider> identity_provider,
      InvalidationServiceOrListenerFactory
          invalidation_service_or_listener_factory = {});
  ProfileInvalidationProvider(const ProfileInvalidationProvider& other) =
      delete;
  ProfileInvalidationProvider& operator=(
      const ProfileInvalidationProvider& other) = delete;
  ~ProfileInvalidationProvider() override;

  // Returns the `InvalidationService` or `InvalidationListener` specific to
  // `sender_id`.
  std::variant<InvalidationService*, InvalidationListener*>
  GetInvalidationServiceOrListener(const std::string& sender_id,
                                   const std::string& project_id);

  IdentityProvider* GetIdentityProvider();

  // KeyedService:
  void Shutdown() override;

  // Register prefs to be used by per-Profile instances of this class which
  // store invalidation state in Profile prefs.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  std::unique_ptr<IdentityProvider> identity_provider_;

  InvalidationServiceOrListenerFactory
      invalidation_service_or_listener_factory_;
  std::map<std::pair<std::string, std::string>,
           std::variant<std::unique_ptr<InvalidationService>,
                        std::unique_ptr<InvalidationListener>>>
      sender_id_to_invalidation_service_or_listener_;
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_PROFILE_INVALIDATION_PROVIDER_H_
