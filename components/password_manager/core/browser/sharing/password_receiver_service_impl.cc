// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/password_receiver_service_impl.h"

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/browser/sharing/incoming_password_sharing_invitation_sync_bridge.h"
#include "components/password_manager/core/browser/sharing/sharing_invitations.h"
#include "components/prefs/pref_service.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync/service/sync_service.h"

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
    const PrefService* pref_service,
    base::RepeatingCallback<syncer::SyncService*(void)> sync_service_getter,
    std::unique_ptr<IncomingPasswordSharingInvitationSyncBridge> sync_bridge,
    PasswordStoreInterface* profile_password_store,
    PasswordStoreInterface* account_password_store)
    : pref_service_(pref_service),
      sync_service_getter_(std::move(sync_service_getter)),
      sync_bridge_(std::move(sync_bridge)),
      profile_password_store_(profile_password_store),
      account_password_store_(account_password_store) {
  CHECK(pref_service_);
  CHECK(profile_password_store_);

  // |sync_bridge_| can be empty in tests.
  if (sync_bridge_) {
    sync_bridge_->SetPasswordReceiverService(this);
  }
}

PasswordReceiverServiceImpl::~PasswordReceiverServiceImpl() = default;

void PasswordReceiverServiceImpl::ProcessIncomingSharingInvitation(
    IncomingSharingInvitation invitation) {
  PasswordStoreInterface* password_store = nullptr;
  // Although at this time, the sync service must exist already since it is
  // responsible for fetching the incoming sharing invitations for the sync
  // server. In case, `sync_service_getter_` return nullptr (e.g. due to a weird
  // corner case of destruction of sync service after delivering the
  // invitation), the user will be considered signed out (i.e.
  // kNotUsingAccountStorage) and hence the invitation will be ignored.
  metrics_util::PasswordAccountStorageUsageLevel usage_level =
      features_util::ComputePasswordAccountStorageUsageLevel(
          pref_service_, sync_service_getter_.Run());
  switch (usage_level) {
    case metrics_util::PasswordAccountStorageUsageLevel::kSyncing:
      password_store = profile_password_store_;
      break;
    case metrics_util::PasswordAccountStorageUsageLevel::kUsingAccountStorage:
      password_store = account_password_store_;
      break;
    case metrics_util::PasswordAccountStorageUsageLevel::
        kNotUsingAccountStorage:
      break;
  }
  // `password_store` shouldn't generally be null, since in those scenarios no
  // invitation should be received at all (e.g. for non sync'ing users). But
  // since it's possible to fully gauarntee that here, this is added as safety
  // net to make sure in such scenarios not passwords are written to any of the
  // stores.
  if (!password_store) {
    return;
  }

  auto task = std::make_unique<ProcessIncomingSharingInvitationTask>(
      std::move(invitation), password_store,
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
