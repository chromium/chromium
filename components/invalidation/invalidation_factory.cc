// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/invalidation_factory.h"

#include <memory>
#include <string>
#include <utility>
#include <variant>

#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/impl/fcm_invalidation_listener.h"
#include "components/invalidation/impl/fcm_invalidation_service.h"
#include "components/invalidation/impl/fcm_network_handler.h"
#include "components/invalidation/impl/per_user_topic_subscription_manager.h"
#include "components/invalidation/invalidation_constants.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/invalidation/public/identity_provider.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace invalidation {

namespace {
constexpr auto kInvalidationProjects = base::MakeFixedFlatSet<std::string_view>(
    {kCriticalInvalidationsProjectNumber,
     kNonCriticalInvalidationsProjectNumber});
}

bool IsInvalidationListenerSupported(std::string_view project_number) {
  return kInvalidationProjects.contains(project_number);
}

std::variant<std::unique_ptr<InvalidationService>,
             std::unique_ptr<InvalidationListener>>
CreateInvalidationServiceOrListener(
    IdentityProvider* identity_provider,
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* pref_service,
    std::string project_number,
    std::string log_prefix) {
  if (IsInvalidationListenerSupported(project_number)) {
    return InvalidationListener::Create(gcm_driver, instance_id_driver,
                                        std::move(project_number),
                                        std::move(log_prefix));
  }

  auto service = std::make_unique<invalidation::FCMInvalidationService>(
      identity_provider,
      base::BindRepeating(&invalidation::FCMNetworkHandler::Create, gcm_driver,
                          instance_id_driver),
      base::BindRepeating(&invalidation::FCMInvalidationListener::Create),
      base::BindRepeating(
          &invalidation::PerUserTopicSubscriptionManager::Create,
          base::RetainedRef(url_loader_factory)),
      instance_id_driver, pref_service, std::move(project_number));
  service->Init();
  return service;
}

}  // namespace invalidation
