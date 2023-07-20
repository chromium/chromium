// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/password_receiver_service_impl.h"

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/browser/sharing/incoming_password_sharing_invitation_sync_bridge.h"
#include "components/password_manager/core/browser/sharing/sharing_invitations.h"
#include "components/sync/model/model_type_controller_delegate.h"

namespace password_manager {

ProcessIncomingSharingInvitationTask::ProcessIncomingSharingInvitationTask(
    IncomingSharingInvitation invitation,
    PasswordStoreInterface* password_store,
    base::OnceCallback<void(ProcessIncomingSharingInvitationTask*)>
        done_callback)
    : invitation_(std::move(invitation)),
      password_store_(password_store),
      done_processing_invitation_callback_(std::move(done_callback)) {
  // Incoming sharing invitation are only accepted if they represent a password
  // form that doesn't exist in the password store. Query the password store
  // first in order to detect existing credentials.
  password_store_->GetLogins(
      PasswordFormDigest(invitation_.scheme, invitation_.signon_realm,
                         invitation_.url),
      weak_ptr_factory_.GetWeakPtr());
}

ProcessIncomingSharingInvitationTask::~ProcessIncomingSharingInvitationTask() =
    default;

void ProcessIncomingSharingInvitationTask::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  for (const std::unique_ptr<PasswordForm>& result : results) {
    // TODO(crbug.com/1448235): process PSL and affilated credentials if needed.
    // TODO(crbug.com/1448235): process conflicting passwords differently if
    // necessary.
    if (result->username_value == invitation_.username_value) {
      std::move(done_processing_invitation_callback_).Run(this);
      return;
    }
  }
  password_store_->AddLogin(
      IncomingSharingInvitationToPasswordForm(invitation_),
      base::BindOnce(std::move(done_processing_invitation_callback_), this));
}

PasswordReceiverServiceImpl::PasswordReceiverServiceImpl(
    std::unique_ptr<IncomingPasswordSharingInvitationSyncBridge> sync_bridge,
    PasswordStoreInterface* password_store)
    : sync_bridge_(std::move(sync_bridge)), password_store_(password_store) {
  CHECK(password_store_);

  // |sync_bridge_| can be empty in tests.
  if (sync_bridge_) {
    sync_bridge_->SetPasswordReceiverService(this);
  }
}

PasswordReceiverServiceImpl::~PasswordReceiverServiceImpl() = default;

void PasswordReceiverServiceImpl::ProcessIncomingSharingInvitation(
    IncomingSharingInvitation invitation) {
  auto task = std::make_unique<ProcessIncomingSharingInvitationTask>(
      std::move(invitation), password_store_,
      /*done_callback=*/
      base::BindOnce(&PasswordReceiverServiceImpl::RemoveTaskFromTasksList,
                     base::Unretained(this)));
  process_invitations_tasks_.push_back(std::move(task));
}

void PasswordReceiverServiceImpl::RemoveTaskFromTasksList(
    ProcessIncomingSharingInvitationTask* task) {
  base::EraseIf(
      process_invitations_tasks_,
      [&task](const std::unique_ptr<ProcessIncomingSharingInvitationTask>&
                  cached_task) { return cached_task.get() == task; });
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
PasswordReceiverServiceImpl::GetControllerDelegate() {
  CHECK(sync_bridge_);
  return sync_bridge_->change_processor()->GetControllerDelegate();
}

}  // namespace password_manager
