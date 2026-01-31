// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/promos/bubble_signin_promo_delegate.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/extensions/sync/account_extension_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/signin/promos/signin_promo_tab_helper.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension_id.h"

namespace {

syncer::DataType GetDataTypeFromAccessPoint(
    signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::AccessPoint::kPasswordBubble:
      return syncer::PASSWORDS;
    case signin_metrics::AccessPoint::kAddressBubble:
      return syncer::CONTACT_INFO;
    case signin_metrics::AccessPoint::kBookmarkBubble:
      return syncer::BOOKMARKS;
    case signin_metrics::AccessPoint::kExtensionInstallBubble:
      return syncer::EXTENSIONS;
    default:
      NOTREACHED();
  }
}

}  // namespace

BubbleSignInPromoDelegate::BubbleSignInPromoDelegate(
    content::WebContents& web_contents,
    signin_metrics::AccessPoint access_point,
    syncer::LocalDataItemModel::DataId data_id)
    : data_id_(std::move(data_id)),
      web_contents_(web_contents.GetWeakPtr()),
      access_point_(access_point) {}

BubbleSignInPromoDelegate::~BubbleSignInPromoDelegate() = default;

void BubbleSignInPromoDelegate::OnSignIn(const AccountInfo& account) {
  // Do not continue if the web contents were destroyed while the bubble was
  // opened.
  if (!web_contents_) {
    return;
  }

  // Signing in is triggered by the user interacting with the sign-in promo.
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext())
          ->GetOriginalProfile();
  CHECK(profile);

  if (!signin::IsSignInPromo(access_point_)) {
    signin_ui_util::EnableSyncFromSingleAccountPromo(profile, account,
                                                     access_point_);
    return;
  }

  base::UmaHistogramEnumeration("Signin.SignInPromo.Accepted", access_point_);
  signin_ui_util::SignInFromSingleAccountPromo(profile, account, access_point_);

  if (!base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp) &&
      access_point_ == signin_metrics::AccessPoint::kExtensionInstallBubble) {
    // Make sure the `data_id_` is of the correct type.
    CHECK(std::holds_alternative<extensions::ExtensionId>(data_id_));
    const extensions::ExtensionId extension_id =
        std::move(std::get<extensions::ExtensionId>(data_id_));

    extensions::AccountExtensionTracker::Get(profile)
        ->OnSignInInitiatedFromExtensionPromo(extension_id);
    return;
  }

  signin_util::SignedInState signed_in_state = signin_util::GetSignedInState(
      IdentityManagerFactory::GetForProfile(profile));

  auto maybe_move_data = base::BindOnce(
      [](Profile* profile, syncer::DataType data_type,
         syncer::LocalDataItemModel::DataId data_id) {
        SyncServiceFactory::GetForProfile(profile)
            ->SelectTypeAndMigrateLocalDataItemsWhenActive(
                data_type, {std::move(data_id)});
      },
      profile, GetDataTypeFromAccessPoint(access_point_), std::move(data_id_));

  // If the sign in was already successful, move the data directly.
  if (signed_in_state == signin_util::SignedInState::kSignedIn) {
    std::move(maybe_move_data).Run();
    return;
  }

  // These states requires a sign in tab to be displayed. A tab helper attached
  // to the tab will take care of the move operation once signed in.
  if (signed_in_state != signin_util::SignedInState::kSignedOut &&
      signed_in_state != signin_util::SignedInState::kSignInPending) {
    return;
  }

  content::WebContents* sign_in_tab_contents =
      signin_ui_util::GetSignInTabWithAccessPoint(
          tabs::TabInterface::GetFromContents(web_contents_.get())
              ->GetBrowserWindowInterface(),
          access_point_);

  // SignInFromSingleAccountPromo may fail to open a tab. Do not wait for a
  // sign in event in that case.
  if (!sign_in_tab_contents) {
    return;
  }

  syncer::SyncUserSettings* sync_user_settings =
      SyncServiceFactory::GetForProfile(profile)->GetUserSettings();
  const std::optional<syncer::UserSelectableType> user_selectable_type =
      GetUserSelectableTypeFromDataType(
          GetDataTypeFromAccessPoint(access_point_));
  CHECK(user_selectable_type.has_value());

  // If the data type was not enabled before, do so directly when the promo
  // is clicked in sign in pending state, rather than waiting for a reauth
  // event for it to be enabled.
  if (base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp) &&
      signed_in_state == signin_util::SignedInState::kSignInPending &&
      !sync_user_settings->GetSelectedTypes().Has(
          user_selectable_type.value())) {
    sync_user_settings->SetSelectedType(user_selectable_type.value(), true);
  }

  SigninPromoTabHelper::GetForWebContents(*sign_in_tab_contents)
      ->InitializeCallbackAfterSignIn(std::move(maybe_move_data),
                                      access_point_);
}
