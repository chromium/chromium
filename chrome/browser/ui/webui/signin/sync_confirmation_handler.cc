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
    Profile* profile,
    const std::unordered_map<std::string, int>& string_to_grd_id_map,
    Browser* browser)
    : profile_(profile),
      string_to_grd_id_map_(string_to_grd_id_map),
      browser_(browser),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile_)) {
  DCHECK(profile_);
  BrowserList::AddObserver(this);
}

SyncConfirmationHandler::~SyncConfirmationHandler() {
  BrowserList::RemoveObserver(this);
  identity_manager_->RemoveObserver(this);

  // Abort signin and prevent sync from starting if none of the actions on the
  // sync confirmation dialog are taken by the user.
  if (!did_user_explicitly_interact_) {
    CloseModalSigninWindow(LoginUIService::UI_CLOSED);
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
      "accountInfoRequest",
      base::BindRepeating(&SyncConfirmationHandler::HandleAccountInfoRequest,
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
  CloseModalSigninWindow(LoginUIService::ABORT_SYNC);
}

void SyncConfirmationHandler::HandleAccountInfoRequest(
    const base::ListValue* args) {
  DCHECK(ProfileSyncServiceFactory::IsSyncAllowed(profile_));
  base::Optional<AccountInfo> primary_account_info =
      identity_manager_->FindExtendedAccountInfoForAccountWithRefreshToken(
          identity_manager_->GetPrimaryAccountInfo(ConsentLevel::kSignin));

  // Fire the "account-info-changed" listener from |SetAccountInfo()|.
  // Note: If the account info is not available yet in the
  // IdentityManager, i.e. account_info is empty, the listener will be
  // fired again through |OnAccountUpdated()|.
  if (primary_account_info && primary_account_info->IsValid())
    SetAccountInfo(*primary_account_info);
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
      identity_manager_->GetPrimaryAccountId(ConsentLevel::kSignin),
      sync_consent);
}

void SyncConfirmationHandler::SetAccountInfo(const AccountInfo& info) {
  DCHECK(info.IsValid());
  if (!ProfileSyncServiceFactory::IsSyncAllowed(profile_)) {
    // The sync disabled confirmation handler does not present the user image.
    // Avoid updating the image URL in this case.
    return;
  }

  GURL picture_gurl(info.picture_url);
  GURL picture_gurl_with_options = signin::GetAvatarImageURLWithOptions(
      picture_gurl, kProfileImageSize, false /* no_silhouette */);

  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey("src", base::Value(picture_gurl_with_options.spec()));
  value.SetKey("showEnterpriseBadge", base::Value(info.IsManaged()));

  AllowJavascript();
  FireWebUIListener("account-info-changed", value);
}

void SyncConfirmationHandler::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  if (!info.IsValid())
    return;

  if (info.account_id !=
      identity_manager_->GetPrimaryAccountId(ConsentLevel::kSignin)) {
    return;
  }

  identity_manager_->RemoveObserver(this);
  SetAccountInfo(info);
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
    case LoginUIService::ABORT_SYNC:
      base::RecordAction(base::UserMetricsAction("Signin_Undo_Signin"));
      break;
    case LoginUIService::UI_CLOSED:
      base::RecordAction(base::UserMetricsAction("Signin_Abort_Signin"));
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

  base::Optional<AccountInfo> primary_account_info =
      identity_manager_->FindExtendedAccountInfoForAccountWithRefreshToken(
          identity_manager_->GetPrimaryAccountInfo(ConsentLevel::kSignin));
  if (!primary_account_info) {
    // No account is signed in, so there is nothing to be displayed in the sync
    // confirmation dialog.
    return;
  }

  if (!primary_account_info->IsValid()) {
    identity_manager_->AddObserver(this);
  } else {
    SetAccountInfo(*primary_account_info);
  }

  if (browser_)
    signin::SetInitializedModalHeight(browser_, web_ui(), args);
}
