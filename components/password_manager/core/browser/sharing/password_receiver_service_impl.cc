// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/password_receiver_service_impl.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/sharing/incoming_password_sharing_invitation_sync_bridge.h"
#include "components/prefs/pref_service.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync/service/sync_service.h"

namespace password_manager {

using metrics_util::ProcessIncomingPasswordSharingInvitationResult;

namespace {

// Copied from components/autofill/core/common/autofill_constants.h.
constexpr size_t kMaxString16Length = 1024;

bool IsValidString16(const std::u16string& str) {
  return str.size() <= kMaxString16Length;
}

bool IsValidString(const std::string& str) {
  return str.size() <= kMaxString16Length;
}

bool IsValidSharedPasswordForm(const PasswordForm& form) {
  if (!form.url.is_valid() || form.url.is_empty()) {
    return false;
  }
  if (!IsValidString16(form.username_element) ||
      !IsValidString16(form.username_value) ||
      !IsValidString16(form.password_element)) {
    return false;
  }
  if (!IsValidString16(form.password_value) || form.password_value.empty()) {
    return false;
  }
  if (!IsValidString(form.signon_realm) || form.signon_realm.empty()) {
    return false;
  }
  if (!IsValidString16(form.display_name) ||
      !IsValidString16(form.sender_name)) {
    return false;
  }
  if (!form.sender_profile_image_url.is_empty() &&
      !form.sender_profile_image_url.is_valid()) {
    return false;
  }
  return true;
}

// Computes the status of processing sharing invitations when the invitation is
// ignored due to having credentials with the same username and origin in the
// password store.
ProcessIncomingPasswordSharingInvitationResult
GetProcessSharingInvitationResultForIgnoredInvitations(
    const PasswordForm& exsiting_credentials,
    const PasswordForm& incoming_credentials) {
  CHECK_EQ(exsiting_credentials.username_value,
           incoming_credentials.username_value);

  if (exsiting_credentials.type != PasswordForm::Type::kReceivedViaSharing) {
    return exsiting_credentials.password_value ==
                   incoming_credentials.password_value
               ? ProcessIncomingPasswordSharingInvitationResult::
                     kCredentialsExistWithSamePassword
               : ProcessIncomingPasswordSharingInvitationResult::
                     kCredentialsExistWithDifferentPassword;
  }

  if (exsiting_credentials.sender_email == incoming_credentials.sender_email) {
    return exsiting_credentials.password_value ==
                   incoming_credentials.password_value
               ? ProcessIncomingPasswordSharingInvitationResult::
                     kSharedCredentialsExistWithSameSenderAndSamePassword
               : ProcessIncomingPasswordSharingInvitationResult::
                     kSharedCredentialsExistWithSameSenderAndDifferentPassword;
  }

  return exsiting_credentials.password_value ==
                 incoming_credentials.password_value
             ? ProcessIncomingPasswordSharingInvitationResult::
                   kSharedCredentialsExistWithDifferentSenderAndSamePassword
             : ProcessIncomingPasswordSharingInvitationResult::
                   kSharedCredentialsExistWithDifferentSenderAndDifferentPassword;
}

// Converts a IncomingPasswordSharingInvitationSpecifics that represents a group
// of credentials to a list of password forms.
std::vector<PasswordForm> IncomingSharingInvitationToPasswordForms(
    const sync_pb::IncomingPasswordSharingInvitationSpecifics& invitation) {
  std::vector<PasswordForm> forms;
  if (!invitation.client_only_unencrypted_data().has_password_group_data()) {
    return forms;
  }

  const sync_pb::PasswordSharingInvitationData::PasswordGroupData&
      incoming_credentials =
          invitation.client_only_unencrypted_data().password_group_data();

  for (const sync_pb::PasswordSharingInvitationData::PasswordGroupElementData&
           password_group_element_data : incoming_credentials.element_data()) {
    PasswordForm form;
    form.username_value =
        base::UTF8ToUTF16(incoming_credentials.username_value());
    form.password_value =
        base::UTF8ToUTF16(incoming_credentials.password_value());

    form.url = GURL(password_group_element_data.origin());
    form.username_element =
        base::UTF8ToUTF16(password_group_element_data.username_element());
    form.password_element =
        base::UTF8ToUTF16(password_group_element_data.password_element());
    form.signon_realm = password_group_element_data.signon_realm();
    form.scheme =
        static_cast<PasswordForm::Scheme>(password_group_element_data.scheme());
    form.display_name =
        base::UTF8ToUTF16(password_group_element_data.display_name());
    form.icon_url = GURL(password_group_element_data.avatar_url());
    form.date_created = base::Time::Now();
    form.type = PasswordForm::Type::kReceivedViaSharing;

    // Invitation metadata.
    const sync_pb::UserDisplayInfo& sender_info =
        invitation.sender_info().user_display_info();
    form.sender_email = base::UTF8ToUTF16(sender_info.email());
    form.sender_name = base::UTF8ToUTF16(sender_info.display_name());
    form.sender_profile_image_url = GURL(sender_info.profile_image_url());

    form.date_received = base::Time::Now();
    form.sharing_notification_displayed = false;

    forms.push_back(std::move(form));
  }

  return forms;
}

}  // namespace

ProcessIncomingSharingInvitationTask::ProcessIncomingSharingInvitationTask(
    PasswordForm incoming_credentials,
    PasswordStoreInterface* password_store,
    base::OnceCallback<void(ProcessIncomingSharingInvitationTask*)>
        done_callback)
    : incoming_credentials_(std::move(incoming_credentials)),
      password_store_(password_store),
      done_processing_invitation_callback_(std::move(done_callback)) {
  // Incoming credentials are only accepted if they represent a password
  // form that doesn't exist in the password store. Query the password store
  // first in order to detect existing credentials.
  password_store_->GetLogins(PasswordFormDigest(incoming_credentials_),
                             weak_ptr_factory_.GetWeakPtr());
}

ProcessIncomingSharingInvitationTask::~ProcessIncomingSharingInvitationTask() =
    default;

void ProcessIncomingSharingInvitationTask::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  // Grouped credentials are ignored because they have different domains.
  std::erase_if(results, [](const auto& form) {
    return form->match_type == PasswordForm::MatchType::kGrouped;
  });
  // TODO(crbug.com/40269204): process PSL and affilated credentials if needed.
  // TODO(crbug.com/40269204): process conflicting passwords differently if
  // necessary.
  auto credential_with_same_username_it = base::ranges::find_if(
      results, [this](const std::unique_ptr<PasswordForm>& result) {
        return result->username_value == incoming_credentials_.username_value;
      });
  if (credential_with_same_username_it == results.end()) {
    metrics_util::LogProcessIncomingPasswordSharingInvitationResult(
        ProcessIncomingPasswordSharingInvitationResult::
            kInvitationAutoApproved);
    password_store_->AddLogin(
        incoming_credentials_,
        base::BindOnce(std::move(done_processing_invitation_callback_), this));
    return;
  }

  ProcessIncomingPasswordSharingInvitationResult processing_result =
      GetProcessSharingInvitationResultForIgnoredInvitations(
          **credential_with_same_username_it, incoming_credentials_);
  metrics_util::LogProcessIncomingPasswordSharingInvitationResult(
      processing_result);

  if (processing_result ==
          ProcessIncomingPasswordSharingInvitationResult::
              kSharedCredentialsExistWithSameSenderAndDifferentPassword &&
      base::FeatureList::IsEnabled(
          features::kAutoApproveSharedPasswordUpdatesFromSameSender)) {
    password_store_->UpdateLogin(
        incoming_credentials_,
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

  // |sync_bridge_| can be empty in tests.
  if (sync_bridge_) {
    sync_bridge_->SetPasswordReceiverService(this);
  }
}

PasswordReceiverServiceImpl::~PasswordReceiverServiceImpl() = default;

void PasswordReceiverServiceImpl::ProcessIncomingSharingInvitation(
    sync_pb::IncomingPasswordSharingInvitationSpecifics invitation) {
  PasswordStoreInterface* password_store = nullptr;
  // Although at this time, the sync service must exist already since it is
  // responsible for fetching the incoming sharing invitations for the sync
  // server. In case, `sync_service_` is null (e.g. due to a weird corner case
  // of destruction of sync service after delivering the invitation), both
  // checks below evaluate to false and hence the invitation will be ignored.
  if (features_util::IsOptedInForAccountStorage(pref_service_, sync_service_)) {
    password_store = account_password_store_;
  } else if (sync_service_ && sync_service_->IsSyncFeatureEnabled()) {
    password_store = profile_password_store_;
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

  std::vector<PasswordForm> incoming_credentials_list =
      IncomingSharingInvitationToPasswordForms(invitation);

  for (const PasswordForm& incoming_credentials : incoming_credentials_list) {
    if (!IsValidSharedPasswordForm(incoming_credentials)) {
      LogProcessIncomingPasswordSharingInvitationResult(
          ProcessIncomingPasswordSharingInvitationResult::kInvalidInvitation);
      continue;
    }
    auto task = std::make_unique<ProcessIncomingSharingInvitationTask>(
        incoming_credentials, password_store,
        /*done_callback=*/
        base::BindOnce(&PasswordReceiverServiceImpl::RemoveTaskFromTasksList,
                       base::Unretained(this)));
    process_invitations_tasks_.push_back(std::move(task));
  }
}

void PasswordReceiverServiceImpl::RemoveTaskFromTasksList(
    ProcessIncomingSharingInvitationTask* task) {
  std::erase_if(
      process_invitations_tasks_,
      [&task](const std::unique_ptr<ProcessIncomingSharingInvitationTask>&
                  cached_task) { return cached_task.get() == task; });
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
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
