// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_PASSWORD_RECEIVER_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_PASSWORD_RECEIVER_SERVICE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/sharing/password_receiver_service_interface.h"
#include "components/password_manager/core/browser/sharing/sharing_invitations.h"

namespace password_manager {

class PasswordStoreInterface;
class IncomingPasswordSharingInvitationSyncBridge;

// A class that represents an in-flight task that processes an incoming password
// sharing invitation. This is necessary since communication with the password
// store is async. This object caches the incoming invtation till getting the
// response from the password store regarding stored credentials such that it
// can process the incoming invitation accordingly.
class ProcessIncomingSharingInvitationTask : public PasswordStoreConsumer {
 public:
  // `done_callback` is invoked when the task is completed passing the value of
  // `this` informing the embedder which task has completed.
  ProcessIncomingSharingInvitationTask(
      IncomingSharingInvitation invitation,
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

  // The invitation that is being processed by this task.
  IncomingSharingInvitation invitation_;

  raw_ptr<PasswordStoreInterface> password_store_;

  base::OnceCallback<void(ProcessIncomingSharingInvitationTask*)>
      done_processing_invitation_callback_;

  base::WeakPtrFactory<ProcessIncomingSharingInvitationTask> weak_ptr_factory_{
      this};
};

// The per-profile service responsible for processing incoming password sharing
// invitations.
class PasswordReceiverService : public KeyedService,
                                public PasswordReceiverServiceInterface {
 public:
  explicit PasswordReceiverService(
      std::unique_ptr<IncomingPasswordSharingInvitationSyncBridge> sync_bridge,
      PasswordStoreInterface* password_store);
  PasswordReceiverService(const PasswordReceiverService&) = delete;
  PasswordReceiverService& operator=(const PasswordReceiverService&) = delete;
  ~PasswordReceiverService() override;

 private:
  // PasswordReceiverServiceInterface implementation:
  void ProcessIncomingSharingInvitation(
      IncomingSharingInvitation invitation) override;

  void RemoveTaskFromTasksList(ProcessIncomingSharingInvitationTask* task);

  std::unique_ptr<IncomingPasswordSharingInvitationSyncBridge> sync_bridge_;

  raw_ptr<PasswordStoreInterface> password_store_;

  // Used to keep track of the currently in-flight tasks processing the incoming
  // password sharing invitations. Once a task is completed it gets removed from
  // the vector and the corresponding task object is destroyed.
  std::vector<std::unique_ptr<ProcessIncomingSharingInvitationTask>>
      process_invitations_tasks_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_PASSWORD_RECEIVER_SERVICE_H_
