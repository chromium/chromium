// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATOR_H_
#define COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATOR_H_

#include "base/callback.h"
#include "base/macros.h"
#include "components/invalidation/impl/fcm_invalidation_listener.h"
#include "components/invalidation/impl/invalidator.h"
#include "components/invalidation/impl/invalidator_registrar.h"
#include "components/invalidation/public/invalidator_state.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

class PrefService;

namespace invalidation {
class IdentityProvider;
}

namespace syncer {

class FCMSyncNetworkChannel;

// This class inplements the Invalidator interface and serves as a
// bridge betwen invalidation Listener and invalidationr Service.
class FCMInvalidator : public Invalidator,
                       public FCMInvalidationListener::Delegate {
 public:
  FCMInvalidator(std::unique_ptr<FCMSyncNetworkChannel> network_channel,
                 invalidation::IdentityProvider* identity_provider,
                 PrefService* pref_service,
                 network::mojom::URLLoaderFactory* loader_factory,
                 const ParseJSONCallback& parse_json);

  ~FCMInvalidator() override;

  // Invalidator implementation.
  void RegisterHandler(InvalidationHandler* handler) override;
  bool UpdateRegisteredIds(InvalidationHandler* handler,
                           const ObjectIdSet& ids) override;
  bool UpdateRegisteredIds(InvalidationHandler* handler,
                           const TopicSet& topics) override;

  void UnregisterHandler(InvalidationHandler* handler) override;
  InvalidatorState GetInvalidatorState() const override;
  void UpdateCredentials(const std::string& email,
                         const std::string& token) override;
  void RequestDetailedStatus(
      base::RepeatingCallback<void(const base::DictionaryValue&)> callback)
      const override;

  // SyncInvalidationListener::Delegate implementation.
  void OnInvalidate(const TopicInvalidationMap& invalidation_map) override;
  void OnInvalidatorStateChange(InvalidatorState state) override;

 private:
  friend class FCMInvalidatorTestDelegate;

  bool is_started_ = false;
  InvalidatorRegistrar registrar_;

  // The invalidation listener.
  FCMInvalidationListener invalidation_listener_;

  DISALLOW_COPY_AND_ASSIGN(FCMInvalidator);
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATOR_H_
