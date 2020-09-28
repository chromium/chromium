// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/sync_confirmation_handler.h"

#include <vector>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/signin_view_controller_delegate.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/signin/public/base/avatar_icon_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"

using signin::ConsentLevel;

namespace {
const int kProfileImageSize = 128;
}  // namespace

SyncConfirmationHandler::SyncConfirmationHandler(
    Browser* browser,
    const std::unordered_map<std::string, int>& string_to_grd_id_map)
    : profile_(browser->profile()),
      browser_(browser),
      string_to_grd_id_map_(string_to_grd_id_map),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile_)) {
  DCHECK(profile_);
  DCHECK(browser_);
  BrowserList::AddObserver(this);
}

SyncConfirmationHandler::~SyncConfirmationHandler() {
  BrowserList::RemoveObserver(this);
  identity_manager_->RemoveObserver(this);

  // Abort signin and prevent sync from starting if none of the actions on the
  // sync confirmation dialog are taken by the user.
  if (!did_user_explicitly_interact_) {
    HandleUndo(nullptr);
    base::RecordAction(base::UserMetricsAction("Signin_Abort_Signin"));
  }
}

void SyncConfirmationHandler::OnBrowserRemoved(Browser* browser) {
  if (browser_ == browser)
    browser_ = nullptr;
}

void SyncConfirmationHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "confirm", base::BindRepeating(&SyncConfirmationHandler::HandleConfirm,
                                     base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "undo", base::BindRepeating(&SyncConfirmationHandler::HandleUndo,
                                  base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "goToSettings",
      base::BindRepeating(&SyncConfirmationHandler::HandleGoToSettings,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "initializedWithSize",
      base::BindRepeating(&SyncConfirmationHandler::HandleInitializedWithSize,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "accountImageRequest",
      base::BindRepeating(&SyncConfirmationHandler::HandleAccountImageRequest,
                          base::Unretained(this)));
}

void SyncConfirmationHandler::HandleConfirm(const base::ListValue* args) {
  did_user_explicitly_interact_ = true;
  RecordConsent(args);
  CloseModalSigninWindow(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
}

void SyncConfirmationHandler::HandleGoToSettings(const base::ListValue* args) {
  DCHECK(ProfileSyncServiceFactory::IsSyncAllowed(profile_));
  did_user_explicitly_interact_ = true;
  RecordConsent(args);
  CloseModalSigninWindow(LoginUIService::CONFIGURE_SYNC_FIRST);
}

void SyncConfirmationHandler::HandleUndo(const base::ListValue* args) {
  did_user_explicitly_interact_ = true;
  CloseModalSigninWindow(LoginUIService::ABORT_SIGNIN);
}

void SyncConfirmationHandler::HandleAccountImageRequest(
    const base::ListValue* args) {
  DCHECK(ProfileSyncServiceFactory::IsSyncAllowed(profile_));
  base::Optional<AccountInfo> primary_account_info =
      identity_manager_->FindExtendedAccountInfoForAccountWithRefreshToken(
          identity_manager_->GetPrimaryAccountInfo(ConsentLevel::kNotRequired));

  // Fire the "account-image-changed" listener from |SetUserImageURL()|.
  // Note: If the account info is not available yet in the
  // IdentityManager, i.e. account_info is empty, the listener will be
  // fired again through |OnAccountUpdated()|.
  if (primary_account_info)
    SetUserImageURL(primary_account_info->picture_url);
}

void SyncConfirmationHandler::RecordConsent(const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  base::Value::ConstListView consent_description = args->GetList()[0].GetList();
  const std::string& consent_confirmation = args->GetList()[1].GetString();

  // The strings returned by the WebUI are not free-form, they must belong into
  // a pre-determined set of strings (stored in |string_to_grd_id_map_|). As
  // this has privacy and legal implications, CHECK the integrity of the strings
  // received from the renderer process before recording the consent.
  std::vector<int> consent_text_ids;
  for (const base::Value& text : consent_description) {
    auto iter = string_to_grd_id_map_.find(text.GetString());
    CHECK(iter != string_to_grd_id_map_.end()) << "Unexpected string:\n"
                                               << text.GetString();
    consent_text_ids.push_back(iter->second);
  }

  auto iter = string_to_grd_id_map_.find(consent_confirmation);
  CHECK(iter != string_to_grd_id_map_.end()) << "Unexpected string:\n"
                                             << consent_confirmation;
  int consent_confirmation_id = iter->second;

  sync_pb::UserConsentTypes::SyncConsent sync_consent;
  sync_consent.set_confirmation_grd_id(consent_confirmation_id);
  for (int id : consent_text_ids) {
    sync_consent.add_description_grd_ids(id);
  }
  sync_consent.set_status(sync_pb::UserConsentTypes::ConsentStatus::
                              UserConsentTypes_ConsentStatus_GIVEN);

  consent_auditor::ConsentAuditor* consent_auditor =
      ConsentAuditorFactory::GetForProfile(profile_);
  consent_auditor->RecordSyncConsent(
      identity_manager_->GetPrimaryAccountId(ConsentLevel::kNotRequired),
      sync_consent);
}

void SyncConfirmationHandler::SetUserImageURL(const std::string& picture_url) {
  if (!ProfileSyncServiceFactory::IsSyncAllowed(profile_)) {
    // The sync disabled confirmation handler does not present the user image.
    // Avoid updating the image URL in this case.
    return;
  }

  std::string picture_url_to_load;
  GURL picture_gurl(picture_url);
  if (picture_gurl.is_valid()) {
    picture_url_to_load =
        signin::GetAvatarImageURLWithOptions(picture_gurl, kProfileImageSize,
                                             false /* no_silhouette */)
            .spec();
  } else {
    // Use the placeholder avatar icon until the account picture URL is fetched.
    picture_url_to_load = profiles::GetPlaceholderAvatarIconUrl();
  }
  base::Value picture_url_value(picture_url_to_load);

  AllowJavascript();
  FireWebUIListener("account-image-changed", picture_url_value);
}

void SyncConfirmationHandler::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  if (!info.IsValid())
    return;

  if (info.account_id !=
      identity_manager_->GetPrimaryAccountId(ConsentLevel::kNotRequired)) {
    return;
  }

  identity_manager_->RemoveObserver(this);
  SetUserImageURL(info.picture_url);
}

void SyncConfirmationHandler::CloseModalSigninWindow(
    LoginUIService::SyncConfirmationUIClosedResult result) {
  switch (result) {
    case LoginUIService::CONFIGURE_SYNC_FIRST:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_WithAdvancedSyncSettings"));
      break;
    case LoginUIService::SYNC_WITH_DEFAULT_SETTINGS:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_WithDefaultSyncSettings"));
      break;
    case LoginUIService::ABORT_SIGNIN:
      base::RecordAction(base::UserMetricsAction("Signin_Undo_Signin"));
      break;
  }
  LoginUIServiceFactory::GetForProfile(profile_)->SyncConfirmationUIClosed(
      result);
  if (browser_)
    browser_->signin_view_controller()->CloseModalSignin();
}

void SyncConfirmationHandler::HandleInitializedWithSize(
    const base::ListValue* args) {
  AllowJavascript();

  if (!browser_)
    return;

  base::Optional<AccountInfo> primary_account_info =
      identity_manager_->FindExtendedAccountInfoForAccountWithRefreshToken(
          identity_manager_->GetPrimaryAccountInfo(ConsentLevel::kNotRequired));
  if (!primary_account_info) {
    // No account is signed in, so there is nothing to be displayed in the sync
    // confirmation dialog.
    return;
  }

  if (!primary_account_info->IsValid()) {
    SetUserImageURL(kNoPictureURLFound);
    identity_manager_->AddObserver(this);
  } else {
    SetUserImageURL(primary_account_info->picture_url);
  }

  signin::SetInitializedModalHeight(browser_, web_ui(), args);
}
