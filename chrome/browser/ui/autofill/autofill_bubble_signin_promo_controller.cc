// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_bubble_signin_promo_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/autofill/autofill_signin_promo_tab_helper.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/common/buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/sync_service.h"

namespace {

syncer::DataType GetDataTypeFromAccessPoint(
    signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::AccessPoint::kPasswordBubble:
      return syncer::PASSWORDS;
    case signin_metrics::AccessPoint::kAddressBubble:
      return syncer::CONTACT_INFO;
    default:
      NOTREACHED();
  }
}

}  // namespace

namespace autofill {

AutofillBubbleSignInPromoController::AutofillBubbleSignInPromoController(
    content::WebContents& web_contents,
    signin_metrics::AccessPoint access_point,
    syncer::LocalDataItemModel::DataId data_id)
    : data_id_(std::move(data_id)),
      web_contents_(web_contents.GetWeakPtr()),
      access_point_(access_point) {}

AutofillBubbleSignInPromoController::~AutofillBubbleSignInPromoController() =
    default;

void AutofillBubbleSignInPromoController::OnSignInToChromeClicked(
    const AccountInfo& account) {
  // Signing in is triggered by the user interacting with the sign-in promo.
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  base::UmaHistogramEnumeration("Signin.SignInPromo.Accepted", access_point_);

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  // TODO(crbug.com/367263145): If a sign in tab already exists, it should
  // update its access point.
  signin_ui_util::SignInFromSingleAccountPromo(profile, account, access_point_);

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

  autofill::AutofillSigninPromoTabHelper::GetForWebContents(
      *sign_in_tab_contents)
      ->InitializeDataMoveAfterSignIn(std::move(maybe_move_data),
                                      access_point_);

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

}  // namespace autofill
