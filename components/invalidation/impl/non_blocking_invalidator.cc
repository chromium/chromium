// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/non_blocking_invalidator.h"

#include <cstddef>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/invalidation/impl/gcm_network_channel_delegate.h"
#include "components/invalidation/impl/invalidation_notifier.h"
#include "components/invalidation/impl/sync_system_resources.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "jingle/notifier/listener/push_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace syncer {

struct NonBlockingInvalidator::InitializeOptions {
  InitializeOptions(
      NetworkChannelCreator network_channel_creator,
      const std::string& invalidator_client_id,
      const UnackedInvalidationsMap& saved_invalidations,
      const std::string& invalidation_bootstrap_data,
      const base::WeakPtr<InvalidationStateTracker>& invalidation_state_tracker,
      const scoped_refptr<base::SingleThreadTaskRunner>&
          invalidation_state_tracker_task_runner,
      const std::string& client_info,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner)
      : network_channel_creator(network_channel_creator),
        invalidator_client_id(invalidator_client_id),
        saved_invalidations(saved_invalidations),
        invalidation_bootstrap_data(invalidation_bootstrap_data),
        invalidation_state_tracker(invalidation_state_tracker),
        invalidation_state_tracker_task_runner(
            invalidation_state_tracker_task_runner),
        client_info(client_info),
        network_task_runner(network_task_runner) {}

  NetworkChannelCreator network_channel_creator;
  std::string invalidator_client_id;
  UnackedInvalidationsMap saved_invalidations;
  std::string invalidation_bootstrap_data;
  base::WeakPtr<InvalidationStateTracker> invalidation_state_tracker;
  scoped_refptr<base::SingleThreadTaskRunner>
      invalidation_state_tracker_task_runner;
  std::string client_info;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner;
};

namespace {
// This class provides a wrapper for a logging class in order to receive
// callbacks across threads, without having to worry about owner threads.
class CallbackProxy {
 public:
  explicit CallbackProxy(
      base::Callback<void(const base::DictionaryValue&)> callback);
  ~CallbackProxy();

  void Run(const base::DictionaryValue& value);

 private:
  static void DoRun(base::Callback<void(const base::DictionaryValue&)> callback,
                    std::unique_ptr<base::DictionaryValue> value);

  base::Callback<void(const base::DictionaryValue&)> callback_;
  scoped_refptr<base::SingleThreadTaskRunner> running_thread_;

  DISALLOW_COPY_AND_ASSIGN(CallbackProxy);
};

CallbackProxy::CallbackProxy(
    base::Callback<void(const base::DictionaryValue&)> callback)
    : callback_(callback),
      running_thread_(base::ThreadTaskRunnerHandle::Get()) {}

CallbackProxy::~CallbackProxy() {}

void CallbackProxy::DoRun(
    base::Callback<void(const base::DictionaryValue&)> callback,
    std::unique_ptr<base::DictionaryValue> value) {
  callback.Run(*value);
}

void CallbackProxy::Run(const base::DictionaryValue& value) {
  std::unique_ptr<base::DictionaryValue> copied(value.DeepCopy());
  running_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&CallbackProxy::DoRun, callback_, std::move(copied)));
}
}

class NonBlockingInvalidator::Core
    : public base::RefCountedThreadSafe<NonBlockingInvalidator::Core>,
      // InvalidationHandler to observe the InvalidationNotifier we create.
      public InvalidationHandler {
 public:
  // Called on parent thread.  |delegate_observer| should be initialized.
  Core(const base::WeakPtr<NonBlockingInvalidator>& delegate_observer,
       const scoped_refptr<base::SingleThreadTaskRunner>&
           delegate_observer_task_runner);

  // Helpers called on I/O thread.
  void Initialize(
      const NonBlockingInvalidator::InitializeOptions& initialize_options);
  void Teardown();
  void UpdateRegisteredIds(const ObjectIdSet& ids);
  void UpdateCredentials(const CoreAccountId& account_id,
                         const std::string& token);
  void RequestDetailedStatus(
      base::Callback<void(const base::DictionaryValue&)> callback) const;

  // InvalidationHandler implementation (all called on I/O thread by
  // InvalidationNotifier).
  void OnInvalidatorStateChange(InvalidatorState reason) override;
  void OnIncomingInvalidation(
      const ObjectIdInvalidationMap& invalidation_map) override;
  std::string GetOwnerName() const override;

 private:
  friend class
      base::RefCountedThreadSafe<NonBlockingInvalidator::Core>;
  // Called on parent or I/O thread.
  ~Core() override;

  // The variables below should be used only on the I/O thread.
  const base::WeakPtr<NonBlockingInvalidator> delegate_observer_;
  scoped_refptr<base::SingleThreadTaskRunner> delegate_observer_task_runner_;
  std::unique_ptr<InvalidationNotifier> invalidation_notifier_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

NonBlockingInvalidator::Core::Core(
    const base::WeakPtr<NonBlockingInvalidator>& delegate_observer,
    const scoped_refptr<base::SingleThreadTaskRunner>&
        delegate_observer_task_runner)
    : delegate_observer_(delegate_observer),
      delegate_observer_task_runner_(delegate_observer_task_runner) {
  DCHECK(delegate_observer_);
  DCHECK(delegate_observer_task_runner_);
}

NonBlockingInvalidator::Core::~Core() {
}

void NonBlockingInvalidator::Core::Initialize(
    const NonBlockingInvalidator::InitializeOptions& initialize_options) {
  DCHECK(initialize_options.network_task_runner);
  network_task_runner_ = initialize_options.network_task_runner;
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  std::unique_ptr<SyncNetworkChannel> network_channel =
      initialize_options.network_channel_creator.Run();
  invalidation_notifier_.reset(new InvalidationNotifier(
      std::move(network_channel), initialize_options.invalidator_client_id,
      initialize_options.saved_invalidations,
      initialize_options.invalidation_bootstrap_data,
      initialize_options.invalidation_state_tracker,
      initialize_options.invalidation_state_tracker_task_runner,
      initialize_options.client_info));
  invalidation_notifier_->RegisterHandler(this);
}

void NonBlockingInvalidator::Core::Teardown() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  invalidation_notifier_->UnregisterHandler(this);
  invalidation_notifier_.reset();
  network_task_runner_ = nullptr;
}

void NonBlockingInvalidator::Core::UpdateRegisteredIds(const ObjectIdSet& ids) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  invalidation_notifier_->UpdateRegisteredIds(this, ids);
}

void NonBlockingInvalidator::Core::UpdateCredentials(
    const CoreAccountId& account_id,
    const std::string& token) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  invalidation_notifier_->UpdateCredentials(account_id, token);
}

void NonBlockingInvalidator::Core::RequestDetailedStatus(
    base::Callback<void(const base::DictionaryValue&)> callback) const {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  invalidation_notifier_->RequestDetailedStatus(callback);
}

void NonBlockingInvalidator::Core::OnInvalidatorStateChange(
    InvalidatorState reason) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  delegate_observer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NonBlockingInvalidator::OnInvalidatorStateChange,
                     delegate_observer_, reason));
}

void NonBlockingInvalidator::Core::OnIncomingInvalidation(
    const ObjectIdInvalidationMap& invalidation_map) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  delegate_observer_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&NonBlockingInvalidator::OnIncomingInvalidation,
                                delegate_observer_, invalidation_map));
}

std::string NonBlockingInvalidator::Core::GetOwnerName() const {
  return "Sync";
}

NonBlockingInvalidator::NonBlockingInvalidator(
    NetworkChannelCreator network_channel_creator,
    const std::string& invalidator_client_id,
    const UnackedInvalidationsMap& saved_invalidations,
    const std::string& invalidation_bootstrap_data,
    InvalidationStateTracker* invalidation_state_tracker,
    const std::string& client_info,
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner)
    : invalidation_state_tracker_(invalidation_state_tracker),
      parent_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      network_task_runner_(network_task_runner) {
  base::WeakPtr<NonBlockingInvalidator> weak_ptr_this =
      weak_ptr_factory_.GetWeakPtr();
  weak_ptr_this.get();  // Bind to this thread.

  core_ = new Core(weak_ptr_this, base::ThreadTaskRunnerHandle::Get());

  InitializeOptions initialize_options(
      network_channel_creator, invalidator_client_id, saved_invalidations,
      invalidation_bootstrap_data, weak_ptr_this,
      base::ThreadTaskRunnerHandle::Get(), client_info, network_task_runner);

  if (!network_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&NonBlockingInvalidator::Core::Initialize,
                                    core_, initialize_options))) {
    NOTREACHED();
  }
}

NonBlockingInvalidator::~NonBlockingInvalidator() {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  if (!network_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&NonBlockingInvalidator::Core::Teardown, core_))) {
    DVLOG(1) << "Network thread stopped before invalidator is destroyed.";
  }
}

void NonBlockingInvalidator::RegisterHandler(InvalidationHandler* handler) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  registrar_.RegisterHandler(handler);
}

bool NonBlockingInvalidator::UpdateRegisteredIds(InvalidationHandler* handler,
                                                 const ObjectIdSet& ids) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  if (!registrar_.UpdateRegisteredIds(handler, ids))
    return false;
  if (!network_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&NonBlockingInvalidator::Core::UpdateRegisteredIds,
                         core_, registrar_.GetAllRegisteredIds()))) {
    NOTREACHED();
  }
  return true;
}

void NonBlockingInvalidator::UnregisterHandler(InvalidationHandler* handler) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  registrar_.UnregisterHandler(handler);
}

InvalidatorState NonBlockingInvalidator::GetInvalidatorState() const {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  return registrar_.GetInvalidatorState();
}

void NonBlockingInvalidator::UpdateCredentials(const CoreAccountId& account_id,
                                               const std::string& token) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  if (!network_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&NonBlockingInvalidator::Core::UpdateCredentials,
                         core_, account_id, token))) {
    NOTREACHED();
  }
}

void NonBlockingInvalidator::RequestDetailedStatus(
    base::Callback<void(const base::DictionaryValue&)> callback) const {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  base::Callback<void(const base::DictionaryValue&)> proxy_callback =
      base::Bind(&CallbackProxy::Run, base::Owned(new CallbackProxy(callback)));
  if (!network_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&NonBlockingInvalidator::Core::RequestDetailedStatus,
                         core_, proxy_callback))) {
    NOTREACHED();
  }
}

NetworkChannelCreator
    NonBlockingInvalidator::MakePushClientChannelCreator(
        const notifier::NotifierOptions& notifier_options) {
  return base::Bind(SyncNetworkChannel::CreatePushClientChannel,
      notifier_options);
}

NetworkChannelCreator NonBlockingInvalidator::MakeGCMNetworkChannelCreator(
    std::unique_ptr<network::SharedURLLoaderFactoryInfo>
        url_loader_factory_info,
    network::NetworkConnectionTracker* network_connection_tracker,
    std::unique_ptr<GCMNetworkChannelDelegate> delegate) {
  return base::Bind(&SyncNetworkChannel::CreateGCMNetworkChannel,
                    base::Passed(&url_loader_factory_info),
                    // NetworkConnectionTracker is a global singleton guaranteed
                    // to be alive when this is used.
                    base::Unretained(network_connection_tracker),
                    base::Passed(&delegate));
}

void NonBlockingInvalidator::ClearAndSetNewClientId(const std::string& data) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  invalidation_state_tracker_->ClearAndSetNewClientId(data);
}

std::string NonBlockingInvalidator::GetInvalidatorClientId() const {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  return invalidation_state_tracker_->GetInvalidatorClientId();
}

void NonBlockingInvalidator::SetBootstrapData(const std::string& data) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  invalidation_state_tracker_->SetBootstrapData(data);
}

std::string NonBlockingInvalidator::GetBootstrapData() const {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  return invalidation_state_tracker_->GetBootstrapData();
}

void NonBlockingInvalidator::SetSavedInvalidations(
      const UnackedInvalidationsMap& states) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  invalidation_state_tracker_->SetSavedInvalidations(states);
}

UnackedInvalidationsMap NonBlockingInvalidator::GetSavedInvalidations() const {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  return invalidation_state_tracker_->GetSavedInvalidations();
}

void NonBlockingInvalidator::Clear() {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  invalidation_state_tracker_->Clear();
}

void NonBlockingInvalidator::OnInvalidatorStateChange(InvalidatorState state) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  registrar_.UpdateInvalidatorState(state);
}

void NonBlockingInvalidator::OnIncomingInvalidation(
        const ObjectIdInvalidationMap& invalidation_map) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  registrar_.DispatchInvalidationsToHandlers(invalidation_map);
}

std::string NonBlockingInvalidator::GetOwnerName() const { return "Sync"; }

}  // namespace syncer
