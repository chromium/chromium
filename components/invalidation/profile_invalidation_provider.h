// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_PROFILE_INVALIDATION_PROVIDER_H_
#define COMPONENTS_INVALIDATION_PROFILE_INVALIDATION_PROVIDER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;

namespace network {
class SharedURLLoaderFactory;
}

namespace invalidation {

class IdentityProvider;
class InvalidationListener;
class LegacyTopicsCleaner;

// A KeyedService that owns `InvalidationListener` instances for project numbers
// (Pantheon project ids).
class ProfileInvalidationProvider : public KeyedService {
 public:
  using InvalidationListenerFactory =
      base::RepeatingCallback<std::unique_ptr<InvalidationListener>(
          int64_t /*project_number*/,
          std::string /*log_prefix*/)>;

  // No-op constructor. Such provider won't return anything on
  // `GetInvalidationListener` call.
  ProfileInvalidationProvider();
  // TODO(crbug.com/341377023): `url_loader_factory`, `identity_provider` and
  // `pref_service` are needed for legacy topics cleanup. Remove it once cleanup
  // is done.
  ProfileInvalidationProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<IdentityProvider> identity_provider,
      PrefService* pref_service,
      InvalidationListenerFactory invalidation_listener_factory);
  ProfileInvalidationProvider(const ProfileInvalidationProvider& other) =
      delete;
  ProfileInvalidationProvider& operator=(
      const ProfileInvalidationProvider& other) = delete;
  ~ProfileInvalidationProvider() override;

  // Returns the `InvalidationListener` specific to `project_number`.
  InvalidationListener* GetInvalidationListener(int64_t project_number);

  // KeyedService:
  void Shutdown() override;

 private:
  InvalidationListenerFactory invalidation_listener_factory_;
  std::map<int64_t, std::unique_ptr<InvalidationListener>>
      project_number_to_invalidation_listener_;

  // Unsubscribes any remaining invalidation topics.
  std::unique_ptr<invalidation::LegacyTopicsCleaner> legacy_topics_cleaner_;
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_PROFILE_INVALIDATION_PROVIDER_H_
