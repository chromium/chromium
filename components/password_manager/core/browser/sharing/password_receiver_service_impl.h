// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_PASSWORD_RECEIVER_SERVICE_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_PASSWORD_RECEIVER_SERVICE_IMPL_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/sharing/password_receiver_service.h"
#include "components/sync/protocol/password_sharing_invitation_specifics.pb.h"
#include "components/sync/service/sync_service_observer.h"

class PrefService;

namespace password_manager {

class PasswordStoreInterface;
class IncomingPasswordSharingInvitationSyncBridge;

// A class that represents an in-flight task that processes an incoming password
// sharing invitation. This is necessary since communication with the password
// store is async. This object caches the incoming credentials till getting the
// response from the password store regarding stored credentials such that it
// can process the incoming invitation accordingly.
class ProcessIncomingSharingInvitationTask : public PasswordStoreConsumer {
 public:
  // `done_callback` is invoked when the task is completed passing the value of
  // `this` informing the embedder which task has completed.
  ProcessIncomingSharingInvitationTask(
      PasswordForm incoming_credentials,
      PasswordStoreInterface* password_store,
      base::OnceCallback<void(ProcessIncomingSharingInvitationTask*)>
          done_callback);
  ProcessIncomingSharingInvitationTask(
      const ProcessIncomingSharingInvitationTask&) = delete;
  ProcessIncomingSharingInvitationTask& operator=(
      const ProcessIncomingSharingInvitationTask&) = delete;
  ~ProcessIncomingSharingInvitationTask() override;

 private:
  // PasswordStoreConsumer implementation:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override;

  // The incoming credentials that are being processed by this task.
  PasswordForm incoming_credentials_;

  raw_ptr<PasswordStoreInterface> password_store_;

  base::OnceCallback<void(ProcessIncomingSharingInvitationTask*)>
      done_processing_invitation_callback_;

  base::WeakPtrFactory<ProcessIncomingSharingInvitationTask> weak_ptr_factory_{
      this};
};

class PasswordReceiverServiceImpl : public PasswordReceiverService,
                                    public syncer::SyncServiceObserver {
 public:
  // Due to the dependency of keyed servivces, the SyncService is only
  // constructed *after* the construction of the PasswordReceiverService, and
  // hence SyncService is provided later in OnSyncServiceInitialized().
  // `sync_bridge`, `profile_password_store` and `account_password_store` may be
  // nullptr in tests.
  explicit PasswordReceiverServiceImpl(
      const PrefService* pref_service,
      std::unique_ptr<IncomingPasswordSharingInvitationSyncBridge> sync_bridge,
      PasswordStoreInterface* profile_password_store,
      PasswordStoreInterface* account_password_store);
  PasswordReceiverServiceImpl(const PasswordReceiverServiceImpl&) = delete;
  PasswordReceiverServiceImpl& operator=(const PasswordReceiverServiceImpl&) =
      delete;
  ~PasswordReceiverServiceImpl() override;

  // PasswordReceiverService implementation:
  void ProcessIncomingSharingInvitation(
      sync_pb::IncomingPasswordSharingInvitationSpecifics invitation) override;
  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate()
      override;
  void OnSyncServiceInitialized(syncer::SyncService* service) override;

  // syncer::SyncServiceObserver overrides.
  void OnSyncShutdown(syncer::SyncService* service) override;

 private:
  void RemoveTaskFromTasksList(ProcessIncomingSharingInvitationTask* task);

  const raw_ptr<const PrefService> pref_service_;

  raw_ptr<syncer::SyncService> sync_service_ = nullptr;

  std::unique_ptr<IncomingPasswordSharingInvitationSyncBridge> sync_bridge_;

  // Both stores can be nullptr in tests.
  raw_ptr<PasswordStoreInterface> profile_password_store_;
  raw_ptr<PasswordStoreInterface> account_password_store_;

  // Used to keep track of the currently in-flight tasks processing the incoming
  // password sharing invitations. Once a task is completed it gets removed from
  // the vector and the corresponding task object is destroyed.
  std::vector<std::unique_ptr<ProcessIncomingSharingInvitationTask>>
      process_invitations_tasks_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_PASSWORD_RECEIVER_SERVICE_IMPL_H_
