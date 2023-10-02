// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/password_receiver_service_impl.h"

#include "base/containers/cxx20_erase.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/browser/sharing/incoming_password_sharing_invitation_sync_bridge.h"
#include "components/password_manager/core/browser/sharing/sharing_invitations.h"
#include "components/prefs/pref_service.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync/service/sync_service.h"

namespace password_manager {

using metrics_util::ProcessIncomingPasswordSharingInvitationResult;

namespace {
// Computes the status of processing sharing invitations when the invitation is
// ignored due to having credentials with the same username and origin in the
// password store.
ProcessIncomingPasswordSharingInvitationResult
GetProcessSharingInvitationResultForIgnoredInvitations(
    const PasswordForm& exsiting_credentials,
    const IncomingSharingInvitation& incoming_invitation) {
  CHECK_EQ(exsiting_credentials.username_value,
           incoming_invitation.username_value);
  if (exsiting_credentials.type != PasswordForm::Type::kReceivedViaSharing) {
    return exsiting_credentials.password_value ==
                   incoming_invitation.password_value
               ? ProcessIncomingPasswordSharingInvitationResult::
                     kCredentialsExistWithSamePassword
               : ProcessIncomingPasswordSharingInvitationResult::
                     kCredentialsExistWithDifferentPassword;
  }

  if (exsiting_credentials.sender_email == incoming_invitation.sender_email) {
    return exsiting_credentials.password_value ==
                   incoming_invitation.password_value
               ? ProcessIncomingPasswordSharingInvitationResult::
                     kSharedCredentialsExistWithSameSenderAndSamePassword
               : ProcessIncomingPasswordSharingInvitationResult::
                     kSharedCredentialsExistWithSameSenderAndDifferentPassword;
  }

  return exsiting_credentials.password_value ==
                 incoming_invitation.password_value
             ? ProcessIncomingPasswordSharingInvitationResult::
                   kSharedCredentialsExistWithDifferentSenderAndSamePassword
             : ProcessIncomingPasswordSharingInvitationResult::
                   kSharedCredentialsExistWithDifferentSenderAndDifferentPassword;
}

}  // namespace

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
  // TODO(crbug.com/1448235): process PSL and affilated credentials if needed.
  // TODO(crbug.com/1448235): process conflicting passwords differently if
  // necessary.
  auto credential_with_same_username_it = base::ranges::find_if(
      results, [this](const std::unique_ptr<PasswordForm>& result) {
        return result->username_value == invitation_.username_value;
      });
  if (credential_with_same_username_it == results.end()) {
    metrics_util::LogProcessIncomingPasswordSharingInvitationResult(
        ProcessIncomingPasswordSharingInvitationResult::
            kInvitationAutoApproved);
    password_store_->AddLogin(
        IncomingSharingInvitationToPasswordForm(invitation_),
        base::BindOnce(std::move(done_processing_invitation_callback_), this));
    return;
  }

  ProcessIncomingPasswordSharingInvitationResult processing_result =
      GetProcessSharingInvitationResultForIgnoredInvitations(
          **credential_with_same_username_it, invitation_);
  metrics_util::LogProcessIncomingPasswordSharingInvitationResult(
      processing_result);

  if (processing_result ==
          ProcessIncomingPasswordSharingInvitationResult::
              kSharedCredentialsExistWithSameSenderAndDifferentPassword &&
      base::FeatureList::IsEnabled(
          features::kAutoApproveSharedPasswordUpdatesFromSameSender)) {
    password_store_->UpdateLogin(
        IncomingSharingInvitationToPasswordForm(invitation_),
        base::BindOnce(std::move(done_processing_invitation_callback_), this));
    return;
  }
  // Run the callback anyway to cleanup the processing task.
  std::move(done_processing_invitation_callback_).Run(this);
}

PasswordReceiverServiceImpl::PasswordReceiverServiceImpl(
    const PrefService* pref_service,
    std::unique_ptr<IncomingPasswordSharingInvitationSyncBridge> sync_bridge,
    PasswordStoreInterface* profile_password_store,
    PasswordStoreInterface* account_password_store)
    : pref_service_(pref_service),
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
  // server. In case, `sync_service_` is null (e.g. due to a weird corner case
  // of destruction of sync service after delivering the invitation), the user
  // will be considered signed out (i.e. kNotUsingAccountStorage) and hence the
  // invitation will be ignored.
  features_util::PasswordAccountStorageUsageLevel usage_level =
      features_util::ComputePasswordAccountStorageUsageLevel(pref_service_,
                                                             sync_service_);
  switch (usage_level) {
    case features_util::PasswordAccountStorageUsageLevel::kSyncing:
      password_store = profile_password_store_;
      break;
    case features_util::PasswordAccountStorageUsageLevel::kUsingAccountStorage:
      password_store = account_password_store_;
      break;
    case features_util::PasswordAccountStorageUsageLevel::
        kNotUsingAccountStorage:
      break;
  }
  // `password_store` shouldn't generally be null, since in those scenarios no
  // invitation should be received at all (e.g. for non sync'ing users). But
  // since it's possible to fully gauarntee that here, this is added as safety
  // net to make sure in such scenarios not passwords are written to any of the
  // stores.
  if (!password_store) {
    LogProcessIncomingPasswordSharingInvitationResult(
        ProcessIncomingPasswordSharingInvitationResult::kNoPasswordStore);
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

void PasswordReceiverServiceImpl::OnSyncServiceInitialized(
    syncer::SyncService* service) {
  CHECK(!sync_service_);
  CHECK(service);
  sync_service_ = service;
  sync_service_->AddObserver(this);
}

void PasswordReceiverServiceImpl::OnSyncShutdown(syncer::SyncService* service) {
  CHECK_EQ(service, sync_service_);
  sync_service_->RemoveObserver(this);
  sync_service_ = nullptr;
}

}  // namespace password_manager
