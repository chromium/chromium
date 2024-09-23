// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/move_to_account_store_bubble_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon/core/favicon_util.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace metrics_util = password_manager::metrics_util;

MoveToAccountStoreBubbleController::MoveToAccountStoreBubbleController(
    base::WeakPtr<PasswordsModelDelegate> delegate)
    : PasswordBubbleControllerBase(
          std::move(delegate),
          password_manager::metrics_util::AUTOMATIC_MOVE_TO_ACCOUNT_STORE) {}

MoveToAccountStoreBubbleController::~MoveToAccountStoreBubbleController() {
  // Make sure the interactions are reported even if Views didn't notify the
  // controller about the bubble being closed.
  OnBubbleClosing();
}

void MoveToAccountStoreBubbleController::RequestFavicon(
    base::OnceCallback<void(const gfx::Image&)> favicon_ready_callback) {
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(GetProfile(),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  favicon::GetFaviconImageForPageURL(
      favicon_service, delegate_->GetPendingPassword().url,
      favicon_base::IconType::kFavicon,
      base::BindOnce(&MoveToAccountStoreBubbleController::OnFaviconReady,
                     base::Unretained(this), std::move(favicon_ready_callback)),
      &favicon_tracker_);
}

void MoveToAccountStoreBubbleController::OnFaviconReady(
    base::OnceCallback<void(const gfx::Image&)> favicon_ready_callback,
    const favicon_base::FaviconImageResult& result) {
  std::move(favicon_ready_callback).Run(result.image);
}

std::u16string MoveToAccountStoreBubbleController::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_SAVE_IN_ACCOUNT_BUBBLE_TITLE);
}

void MoveToAccountStoreBubbleController::AcceptMove() {
  dismissal_reason_ = metrics_util::CLICKED_ACCEPT;
  if (!delegate_->GetPasswordFeatureManager()->IsOptedInForAccountStorage()) {
    // The user opted out since the bubble was shown. This should be rare and
    // ultimately harmless, just do nothing.
    return;
  }
  return delegate_->MovePasswordToAccountStore();
}

void MoveToAccountStoreBubbleController::RejectMove() {
  if (delegate_->GetState() ==
      password_manager::ui::MOVE_CREDENTIAL_AFTER_LOG_IN_STATE) {
    dismissal_reason_ = metrics_util::CLICKED_NEVER;
    return delegate_->BlockMovingPasswordToAccountStore();
  }
  dismissal_reason_ = metrics_util::CLICKED_CANCEL;
}

gfx::Image MoveToAccountStoreBubbleController::GetProfileIcon(int size) {
  if (!GetProfile()) {
    return gfx::Image();
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile());
  if (!identity_manager) {
    return gfx::Image();
  }
  AccountInfo primary_account_info = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  DCHECK(!primary_account_info.IsEmpty());

  gfx::Image account_icon = primary_account_info.account_image;
  if (account_icon.IsEmpty()) {
    account_icon = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        profiles::GetPlaceholderAvatarIconResourceID());
  }
  return profiles::GetSizedAvatarIcon(account_icon,
                                      /*width=*/size, /*height=*/size,
                                      profiles::SHAPE_CIRCLE);
}

std::u16string MoveToAccountStoreBubbleController::GetProfileEmail() const {
  if (!GetProfile()) {
    return std::u16string();
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile());
  if (!identity_manager) {
    return std::u16string();
  }

  return base::UTF8ToUTF16(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email);
}

void MoveToAccountStoreBubbleController::ReportInteractions() {
  Profile* profile = GetProfile();
  if (!profile) {
    return;
  }

  metrics_util::LogMoveUIDismissalReason(
      dismissal_reason_,
      password_manager::features_util::ComputePasswordAccountStorageUserState(
          profile->GetPrefs(), SyncServiceFactory::GetForProfile(profile)));
  // TODO(crbug.com/40123455): Consider recording UKM here, via:
  // metrics_recorder_->RecordUIDismissalReason(dismissal_reason_)
}
