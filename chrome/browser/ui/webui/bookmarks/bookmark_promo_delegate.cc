// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/bookmarks/bookmark_promo_delegate.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/signin/account_preview_utils.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_preview_data_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/sync_prefs.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

constexpr int kBatchUploadBookmarkPromoMaxDismissCount = 3;
constexpr base::TimeDelta
    kBatchUploadBookmarkPromoMinimumDelayToShowAfterDismiss = base::Days(7);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
constexpr int kBookmarkManagerSigninPromoMaxImpressionCount = 5;
constexpr int kBookmarkManagerSigninPromoMaxDismissCount = 2;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

GaiaId GetPrimaryAccountGaiaId(signin::IdentityManager* identity_manager) {
  if (!identity_manager) {
    return GaiaId();
  }
  return identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
      .gaia;
}

}  // namespace

BookmarkPromoData::BookmarkPromoData() = default;
BookmarkPromoData::~BookmarkPromoData() = default;
BookmarkPromoData::BookmarkPromoData(const BookmarkPromoData&) = default;
BookmarkPromoData& BookmarkPromoData::operator=(const BookmarkPromoData&) =
    default;
BookmarkPromoData::BookmarkPromoData(BookmarkPromoData&&) = default;
BookmarkPromoData& BookmarkPromoData::operator=(BookmarkPromoData&&) = default;

base::DictValue BookmarkPromoData::ToDict() const {
  base::DictValue dict;
  dict.Set("promoType", static_cast<int>(promo_type));
  dict.Set("canShow", can_show);
  dict.Set("promoTitle", promo_title);
  dict.Set("promoSubtitle", promo_subtitle);
  dict.Set("actionButtonText", action_button_text);
  dict.Set("promoAvatarUrl", promo_avatar_url);
  return dict;
}

BatchUploadPromoDelegate::BatchUploadPromoDelegate(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service,
    BatchUploadService* batch_upload_service)
    : pref_service_(pref_service),
      identity_manager_(identity_manager),
      sync_service_(sync_service),
      batch_upload_service_(batch_upload_service) {}

BatchUploadPromoDelegate::~BatchUploadPromoDelegate() = default;

bool BatchUploadPromoDelegate::CanShowPromo() const {
  if (!pref_service_ || !identity_manager_ || !sync_service_ ||
      sync_service_->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY) ||
      !batch_upload_service_) {
    return false;
  }

  if (!batch_upload_service_->CanShowPromo(
          BatchUploadService::EntryPoint::kBookmarksManagerPromoCard)) {
    return false;
  }

  GaiaId gaia_id = GetPrimaryAccountGaiaId(identity_manager_);
  if (gaia_id.empty()) {
    return false;
  }

  auto [dismiss_count, last_dismiss_time] =
      SigninPrefs(*pref_service_)
          .GetBookmarkBatchUploadPromoDismissCountWithLastTime(gaia_id);

  if (dismiss_count > kBatchUploadBookmarkPromoMaxDismissCount) {
    return false;
  }

  // If no dismiss were recorded yet, then the promo can always be shown.
  // Otherwise, we can only show the promo if a minimum duration has passed
  // since the last dismiss time.
  return !last_dismiss_time.has_value() ||
         (base::Time::Now() - last_dismiss_time.value() >
          kBatchUploadBookmarkPromoMinimumDelayToShowAfterDismiss);
}

void BatchUploadPromoDelegate::GetPromoData(PromoDataCallback callback) const {
  if (!CanShowPromo()) {
    std::move(callback).Run(BookmarkPromoData());
    return;
  }

  CHECK(batch_upload_service_);
  batch_upload_service_->GetLocalDataDescriptionsForAvailableTypes(
      base::BindOnce(&BatchUploadPromoDelegate::OnLocalDataDescriptionsReceived,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BatchUploadPromoDelegate::OnLocalDataDescriptionsReceived(
    PromoDataCallback callback,
    std::map<syncer::DataType, syncer::LocalDataDescription> local_data) const {
  int local_bookmark_count =
      local_data.contains(syncer::BOOKMARKS)
          ? local_data.at(syncer::BOOKMARKS).local_data_models.size()
          : 0;

  bool has_non_bookmark_local_data = std::ranges::any_of(
      local_data, [](const std::pair<syncer::DataType,
                                     syncer::LocalDataDescription>& data) {
        return data.first != syncer::BOOKMARKS &&
               !data.second.local_data_models.empty();
      });

  BookmarkPromoData promo_data;
  promo_data.can_show = local_bookmark_count != 0 && CanShowPromo();
  promo_data.promo_type = promo_data.can_show ? BookmarkPromoType::kBatchUpload
                                              : BookmarkPromoType::kNone;
  promo_data.promo_title =
      l10n_util::GetStringUTF16(IDS_BATCH_UPLOAD_PROMO_TITLE);
  promo_data.promo_subtitle = l10n_util::GetPluralStringFUTF16(
      has_non_bookmark_local_data
          ? IDS_BATCH_UPLOAD_PROMO_SUBTITLE_BOOKMARKS_COMBO
          : IDS_BATCH_UPLOAD_PROMO_SUBTITLE_BOOKMARKS,
      local_bookmark_count);
  promo_data.action_button_text =
      l10n_util::GetStringUTF16(IDS_BATCH_UPLOAD_PROMO_TITLE_OK_BUTTON_LABEL);
  std::move(callback).Run(std::move(promo_data));
}

void BatchUploadPromoDelegate::OnPromoClicked(
    BrowserWindowInterface* browser_window) {
  if (!CanShowPromo() || !browser_window) {
    return;
  }
  batch_upload_service_->OpenBatchUpload(
      browser_window,
      BatchUploadService::EntryPoint::kBookmarksManagerPromoCard);
}

void BatchUploadPromoDelegate::OnPromoDismissed() {
  if (!pref_service_) {
    return;
  }
  GaiaId gaia_id = GetPrimaryAccountGaiaId(identity_manager_);
  if (!gaia_id.empty()) {
    SigninPrefs(*pref_service_)
        .IncrementBookmarkBatchUploadPromoDismissCountWithLastTime(gaia_id);
  }
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
AccountAwareSignInPromoDelegate::AccountAwareSignInPromoDelegate(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service,
    signin::AccountPreviewDataService* account_preview_data_service)
    : pref_service_(pref_service),
      identity_manager_(identity_manager),
      sync_service_(sync_service),
      account_preview_data_service_(account_preview_data_service) {}

AccountAwareSignInPromoDelegate::~AccountAwareSignInPromoDelegate() = default;

bool AccountAwareSignInPromoDelegate::CanShowPromo() const {
  if (dismissed_in_session_ || !pref_service_ || !identity_manager_ ||
      !sync_service_ || !account_preview_data_service_ ||
      !base::FeatureList::IsEnabled(
          switches::kEnableAccountPreviewPreferredAccountFollowup) ||
      sync_service_->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY) ||
      syncer::SyncPrefs(pref_service_).IsLocalSyncEnabled() ||
      !pref_service_->GetBoolean(prefs::kSigninAllowed)) {
    return false;
  }

  if (sync_service_->GetUserSettings()->IsTypeManagedByPolicy(
          syncer::UserSelectableType::kBookmarks) ||
      !sync_service_->GetDataTypesForTransportOnlyMode().Has(
          syncer::BOOKMARKS)) {
    return false;
  }

  if (identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return false;
  }

  AccountInfo account_info = signin_ui_util::GetSingleAccountForPromos(
      identity_manager_, account_preview_data_service_);
  if (account_info.IsEmpty()) {
    return false;
  }

  std::optional<signin::AccountPreviewDataService::AccountPreviewPreference>
      preferred_account =
          account_preview_data_service_->GetPreferredAccountForPromo();
  if (!preferred_account.has_value() ||
      preferred_account->gaia_id != account_info.GetGaiaId()) {
    return false;
  }

  bool has_bookmarks_in_preview = std::ranges::any_of(
      preferred_account->preferred_data_types,
      [](const signin::PreferredDataTypeInfo& info) {
        return info.data_type == syncer::BOOKMARKS &&
               info.quartile != signin::SyncDataQuartile::kZero;
      });
  if (!has_bookmarks_in_preview) {
    return false;
  }

  SigninPrefs signin_prefs(*pref_service_);
  if (signin_prefs.GetBookmarkManagerSigninPromoDismissCount(
          account_info.GetGaiaId()) >=
      kBookmarkManagerSigninPromoMaxDismissCount) {
    return false;
  }

  if (shown_for_gaia_id_ != account_info.GetGaiaId() &&
      signin_prefs.GetBookmarkManagerSigninPromoImpressionCount(
          account_info.GetGaiaId()) >=
          kBookmarkManagerSigninPromoMaxImpressionCount) {
    return false;
  }

  return true;
}

void AccountAwareSignInPromoDelegate::GetPromoData(
    PromoDataCallback callback) const {
  if (!CanShowPromo()) {
    std::move(callback).Run(BookmarkPromoData());
    return;
  }

  AccountInfo account_info = signin_ui_util::GetSingleAccountForPromos(
      identity_manager_, account_preview_data_service_);
  CHECK(!account_info.IsEmpty());

  std::optional<signin::AccountPreviewDataService::AccountPreviewPreference>
      preferred_account =
          account_preview_data_service_->GetPreferredAccountForPromo();
  CHECK(preferred_account.has_value());

  BookmarkPromoData promo_data;
  promo_data.promo_type = BookmarkPromoType::kAccountAwareSignIn;
  promo_data.can_show = true;
  promo_data.promo_title =
      signin::GetAccountPreviewBookmarkManagerPromoTitle(*preferred_account);
  promo_data.promo_subtitle =
      l10n_util::GetStringFUTF16(IDS_BOOKMARK_MANAGER_SIGNIN_PROMO_SUBTITLE,
                                 base::UTF8ToUTF16(account_info.GetEmail()));
  promo_data.action_button_text = l10n_util::GetStringFUTF16(
      IDS_PROFILES_DICE_WEB_ONLY_SIGNIN_BUTTON,
      base::UTF8ToUTF16(
          account_info.GetGivenName().value_or(account_info.GetEmail())));
  promo_data.promo_avatar_url = signin::GetAccountPictureUrl(account_info);
  std::move(callback).Run(std::move(promo_data));
}

void AccountAwareSignInPromoDelegate::OnPromoShown() {
  if (!CanShowPromo()) {
    return;
  }
  AccountInfo account_info = signin_ui_util::GetSingleAccountForPromos(
      identity_manager_, account_preview_data_service_);
  if (!account_info.IsEmpty() &&
      shown_for_gaia_id_ != account_info.GetGaiaId()) {
    SigninPrefs(*pref_service_)
        .IncrementBookmarkManagerSigninPromoImpressionCount(
            account_info.GetGaiaId());
    signin_metrics::LogSignInOffered(
        signin_metrics::AccessPoint::kBookmarkManager,
        signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT);
    shown_for_gaia_id_ = account_info.GetGaiaId();
  }
}

void AccountAwareSignInPromoDelegate::OnPromoClicked(
    BrowserWindowInterface* browser_window) {
  if (!CanShowPromo() || !browser_window) {
    return;
  }
  AccountInfo account_info = signin_ui_util::GetSingleAccountForPromos(
      identity_manager_, account_preview_data_service_);
  signin_ui_util::SignInFromSingleAccountPromo(
      browser_window->GetProfile(), account_info.GetCoreAccountInfo(),
      signin_metrics::AccessPoint::kBookmarkManager);
}

void AccountAwareSignInPromoDelegate::OnPromoDismissed() {
  dismissed_in_session_ = true;
  if (!pref_service_ || !identity_manager_) {
    return;
  }
  AccountInfo account_info = signin_ui_util::GetSingleAccountForPromos(
      identity_manager_, account_preview_data_service_);
  GaiaId promo_gaia_id = !account_info.GetGaiaId().empty()
                             ? account_info.GetGaiaId()
                             : shown_for_gaia_id_;
  if (!promo_gaia_id.empty()) {
    SigninPrefs(*pref_service_)
        .IncrementBookmarkManagerSigninPromoDismissCount(promo_gaia_id);
  }
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
