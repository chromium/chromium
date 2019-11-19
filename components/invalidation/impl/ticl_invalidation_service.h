// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_TICL_INVALIDATION_SERVICE_H_
#define COMPONENTS_INVALIDATION_IMPL_TICL_INVALIDATION_SERVICE_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/invalidation/impl/deprecated_invalidator_registrar.h"
#include "components/invalidation/impl/invalidation_logger.h"
#include "components/invalidation/public/identity_provider.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom.h"

namespace gcm {
class GCMDriver;
}

namespace network {
class NetworkConnectionTracker;
class SharedURLLoaderFactory;
}

namespace syncer {
class InvalidationStateTracker;
class Invalidator;
}

namespace invalidation {
class GCMInvalidationBridge;

// This InvalidationService wraps the C++ Invalidation Client (TICL) library.
// It provides invalidations for desktop platforms (Win, Mac, Linux).
class TiclInvalidationService : public InvalidationService,
                                public IdentityProvider::Observer,
                                public syncer::InvalidationHandler {
 public:
  TiclInvalidationService(
      const std::string& user_agent,
      IdentityProvider* identity_provider,
      gcm::GCMDriver* gcm_driver,
      // |get_socket_factory_callback| will be safe to call on the IO thread,
      // but will check its WeakPtr parameter on the UI thread.
      base::RepeatingCallback<void(
          base::WeakPtr<TiclInvalidationService>,
          mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>)>
          get_socket_factory_callback,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      network::NetworkConnectionTracker* network_connection_tracker);
  ~TiclInvalidationService() override;

  void Init(std::unique_ptr<syncer::InvalidationStateTracker>
                invalidation_state_tracker);

  // InvalidationService implementation.
  // It is an error to have registered handlers when the service is destroyed.
  void RegisterInvalidationHandler(
      syncer::InvalidationHandler* handler) override;
  bool UpdateRegisteredInvalidationIds(syncer::InvalidationHandler* handler,
                                       const syncer::ObjectIdSet& ids) override;
  void UnregisterInvalidationHandler(
      syncer::InvalidationHandler* handler) override;
  syncer::InvalidatorState GetInvalidatorState() const override;
  std::string GetInvalidatorClientId() const override;
  InvalidationLogger* GetInvalidationLogger() override;
  void RequestDetailedStatus(
      base::Callback<void(const base::DictionaryValue&)> caller) const override;

  void RequestAccessToken();

  void OnAccessTokenRequestCompleted(GoogleServiceAuthError error,
                                     std::string access_token);
  void OnAccessTokenRequestSucceeded(std::string access_token);
  void OnAccessTokenRequestFailed(GoogleServiceAuthError error);

  // IdentityProvider::Observer implementation.
  void OnActiveAccountRefreshTokenUpdated() override;
  void OnActiveAccountRefreshTokenRemoved() override;
  void OnActiveAccountLogin() override;
  void OnActiveAccountLogout() override;

  // syncer::InvalidationHandler implementation.
  void OnInvalidatorStateChange(syncer::InvalidatorState state) override;
  void OnIncomingInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map) override;
  std::string GetOwnerName() const override;

 protected:
  // Initializes with an injected invalidator.
  void InitForTest(std::unique_ptr<syncer::InvalidationStateTracker>
                       invalidation_state_tracker,
                   syncer::Invalidator* invalidator);

 private:
  friend class TiclInvalidationServiceTestDelegate;

  bool IsReadyToStart();
  bool IsStarted() const;

  void StartInvalidator();
  void UpdateInvalidatorCredentials();
  void StopInvalidator();

  const std::string user_agent_;

  IdentityProvider* identity_provider_;

  std::unique_ptr<syncer::DeprecatedInvalidatorRegistrar>
      invalidator_registrar_;
  std::unique_ptr<syncer::InvalidationStateTracker> invalidation_state_tracker_;
  std::unique_ptr<syncer::Invalidator> invalidator_;

  // TiclInvalidationService needs to remember access token in order to
  // invalidate it with IdentityProvider.
  std::string access_token_;

  // TiclInvalidationService needs to hold reference to access_token_fetcher_
  // for the duration of request in order to receive callbacks.
  std::unique_ptr<ActiveAccountAccessTokenFetcher> access_token_fetcher_;
  base::OneShotTimer request_access_token_retry_timer_;
  net::BackoffEntry request_access_token_backoff_;

  gcm::GCMDriver* gcm_driver_;
  std::unique_ptr<GCMInvalidationBridge> gcm_invalidation_bridge_;
  base::RepeatingCallback<void(
      mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>)>
      get_socket_factory_callback_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  network::NetworkConnectionTracker* network_connection_tracker_;

  // The invalidation logger object we use to record state changes
  // and invalidations.
  InvalidationLogger logger_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used on the UI thread.
  base::WeakPtrFactory<TiclInvalidationService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TiclInvalidationService);
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_TICL_INVALIDATION_SERVICE_H_
