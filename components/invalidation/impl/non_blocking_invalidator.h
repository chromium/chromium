// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An implementation of SyncNotifier that wraps InvalidationNotifier
// on its own thread.

#ifndef COMPONENTS_INVALIDATION_IMPL_NON_BLOCKING_INVALIDATOR_H_
#define COMPONENTS_INVALIDATION_IMPL_NON_BLOCKING_INVALIDATOR_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/invalidation/impl/deprecated_invalidator_registrar.h"
#include "components/invalidation/impl/invalidation_state_tracker.h"
#include "components/invalidation/impl/invalidator.h"
#include "components/invalidation/impl/unacked_invalidation_set.h"
#include "components/invalidation/public/invalidation_export.h"
#include "components/invalidation/public/invalidator_state.h"
#include "jingle/notifier/base/notifier_options.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace network {
class NetworkConnectionTracker;
class SharedURLLoaderFactoryInfo;
}  // namespace network

namespace syncer {
class SyncNetworkChannel;
class GCMNetworkChannelDelegate;

// Callback type for function that creates SyncNetworkChannel. This function
// gets passed into NonBlockingInvalidator constructor.
using NetworkChannelCreator =
    base::Callback<std::unique_ptr<SyncNetworkChannel>(void)>;

class INVALIDATION_EXPORT NonBlockingInvalidator
    : public Invalidator,
      public InvalidationStateTracker {
 public:
  // |invalidation_state_tracker| must be initialized and must outlive |this|.
  NonBlockingInvalidator(
      NetworkChannelCreator network_channel_creator,
      const std::string& invalidator_client_id,
      const UnackedInvalidationsMap& saved_invalidations,
      const std::string& invalidation_bootstrap_data,
      InvalidationStateTracker* invalidation_state_tracker,
      const std::string& client_info,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner);

  ~NonBlockingInvalidator() override;

  // Invalidator implementation.
  void RegisterHandler(InvalidationHandler* handler) override;
  bool UpdateRegisteredIds(InvalidationHandler* handler,
                           const ObjectIdSet& ids) override;
  void UnregisterHandler(InvalidationHandler* handler) override;
  InvalidatorState GetInvalidatorState() const override;
  void UpdateCredentials(const CoreAccountId& account_id,
                         const std::string& token) override;
  void RequestDetailedStatus(base::Callback<void(const base::DictionaryValue&)>
                                 callback) const override;

  // Static functions to construct callback that creates network channel for
  // SyncSystemResources. The goal is to pass network channel to invalidator at
  // the same time not exposing channel specific parameters to invalidator and
  // channel implementation to client of invalidator.
  static NetworkChannelCreator MakePushClientChannelCreator(
      const notifier::NotifierOptions& notifier_options);
  static NetworkChannelCreator MakeGCMNetworkChannelCreator(
      std::unique_ptr<network::SharedURLLoaderFactoryInfo>
          url_loader_factory_info,
      network::NetworkConnectionTracker* network_connection_tracker,
      std::unique_ptr<GCMNetworkChannelDelegate> delegate);

  // These methods are forwarded to the invalidation_state_tracker_.
  void ClearAndSetNewClientId(const std::string& data) override;
  std::string GetInvalidatorClientId() const override;
  void SetBootstrapData(const std::string& data) override;
  std::string GetBootstrapData() const override;
  void SetSavedInvalidations(const UnackedInvalidationsMap& states) override;
  UnackedInvalidationsMap GetSavedInvalidations() const override;
  void Clear() override;

 private:
  void OnInvalidatorStateChange(InvalidatorState state);
  void OnIncomingInvalidation(const ObjectIdInvalidationMap& invalidation_map);
  std::string GetOwnerName() const;

  friend class NonBlockingInvalidatorTestDelegate;

  struct InitializeOptions;
  class Core;

  DeprecatedInvalidatorRegistrar registrar_;
  InvalidationStateTracker* invalidation_state_tracker_;

  // The real guts of NonBlockingInvalidator, which allows this class to live
  // completely on the parent thread.
  scoped_refptr<Core> core_;
  scoped_refptr<base::SingleThreadTaskRunner> parent_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  base::WeakPtrFactory<NonBlockingInvalidator> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NonBlockingInvalidator);
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_IMPL_NON_BLOCKING_INVALIDATOR_H_
