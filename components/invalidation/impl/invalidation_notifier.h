// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An implementation of Invalidator that wraps an invalidation
// client.  Handles the details of connecting to XMPP and hooking it
// up to the invalidation client.
//
// You probably don't want to use this directly; use
// NonBlockingInvalidator.

#ifndef COMPONENTS_INVALIDATION_IMPL_INVALIDATION_NOTIFIER_H_
#define COMPONENTS_INVALIDATION_IMPL_INVALIDATION_NOTIFIER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "components/invalidation/impl/deprecated_invalidator_registrar.h"
#include "components/invalidation/impl/invalidation_state_tracker.h"
#include "components/invalidation/impl/invalidator.h"
#include "components/invalidation/impl/sync_invalidation_listener.h"
#include "components/invalidation/public/invalidation_export.h"

namespace syncer {

// This class must live on the IO thread.
class INVALIDATION_EXPORT InvalidationNotifier
    : public Invalidator,
      public SyncInvalidationListener::Delegate {
 public:
  // |invalidation_state_tracker| must be initialized.
  InvalidationNotifier(
      std::unique_ptr<SyncNetworkChannel> network_channel,
      const std::string& invalidator_client_id,
      const UnackedInvalidationsMap& saved_invalidations,
      const std::string& invalidation_bootstrap_data,
      const base::WeakPtr<InvalidationStateTracker>& invalidation_state_tracker,
      scoped_refptr<base::SingleThreadTaskRunner>
          invalidation_state_tracker_task_runner,
      const std::string& client_info);

  ~InvalidationNotifier() override;

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

  // SyncInvalidationListener::Delegate implementation.
  void OnInvalidate(const ObjectIdInvalidationMap& invalidation_map) override;
  void OnInvalidatorStateChange(InvalidatorState state) override;

 private:
  // We start off in the STOPPED state.  When we get our initial
  // credentials, we connect and move to the CONNECTING state.  When
  // we're connected we start the invalidation client and move to the
  // STARTED state.  We never go back to a previous state.
  enum State {
    STOPPED,
    CONNECTING,
    STARTED
  };
  State state_;

  DeprecatedInvalidatorRegistrar registrar_;

  // Passed to |invalidation_listener_|.
  const UnackedInvalidationsMap saved_invalidations_;

  // Passed to |invalidation_listener_|.
  const base::WeakPtr<InvalidationStateTracker> invalidation_state_tracker_;
  scoped_refptr<base::SequencedTaskRunner>
      invalidation_state_tracker_task_runner_;

  // Passed to |invalidation_listener_|.
  const std::string client_info_;

  // The client ID to pass to |invalidation_listener_|.
  const std::string invalidator_client_id_;

  // The initial bootstrap data to pass to |invalidation_listener_|.
  const std::string invalidation_bootstrap_data_;

  // The invalidation listener.
  SyncInvalidationListener invalidation_listener_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(InvalidationNotifier);
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_IMPL_INVALIDATION_NOTIFIER_H_
