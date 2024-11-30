// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_bubble_signin_promo_controller.h"

#include "base/containers/fixed_flat_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/autofill/autofill_signin_promo_tab_helper.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/common/buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"

namespace {
// Modify this when sign in promos for other data types are added.
constexpr auto kAccessPointToDataTypeMap =
    base::MakeFixedFlatMap<signin_metrics::AccessPoint, syncer::DataType>(
        {{signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE,
          syncer::PASSWORDS},
         {signin_metrics::AccessPoint::ACCESS_POINT_ADDRESS_BUBBLE,
          syncer::CONTACT_INFO}});

// Used for attaching the uploader to a profile.
const void* const kSignInPromoDataUploaderChecker =
    &kSignInPromoDataUploaderChecker;

// Used to perform the necessary preliminary checks to know whether we can
// proceed with the upload of the data to account store through the
// `move_callback`. Depending on the sync service's status, it may wait for the
// sync service to be initialized, which is necessary since we may have just
// signed in.
class SignInPromoDataUploaderChecker : public base::SupportsUserData::Data,
                                       public syncer::SyncServiceObserver {
 public:
  explicit SignInPromoDataUploaderChecker(
      content::WebContents* web_contents,
      Profile* profile,
      syncer::DataType data_type,
      base::OnceCallback<void(content::WebContents*)> move_callback)
      : web_contents_(web_contents->GetWeakPtr()),
        profile_(profile->GetWeakPtr()),
        data_type_(data_type),
        move_callback_(std::move(move_callback)) {
    syncer::SyncService* sync_service =
        SyncServiceFactory::GetForProfile(profile_.get());

    // If the data type is already available, move the data right away. If not,
    // wait for the sync service to be activated.
    if (sync_service->GetActiveDataTypes().Has(data_type_) && web_contents_) {
      std::move(move_callback_).Run(web_contents_.get());
      profile_->RemoveUserData(kSignInPromoDataUploaderChecker);
      return;
    }

    sync_service_observer_.Observe(sync_service);
  }
  ~SignInPromoDataUploaderChecker() override = default;

  void OnStateChanged(syncer::SyncService* sync_service) override {
    // Do nothing if the sync service is not ready yet.
    if (sync_service->GetTransportState() !=
        syncer::SyncService::TransportState::ACTIVE) {
      return;
    }

    // If the data type is available in the sync service, move the data to the
    // account store. If not, keep it in local storage.
    if (sync_service->GetActiveDataTypes().Has(data_type_) && web_contents_) {
      std::move(move_callback_).Run(web_contents_.get());
    }

    sync_service_observer_.Reset();
    profile_->RemoveUserData(kSignInPromoDataUploaderChecker);
  }

  void OnSyncShutdown(syncer::SyncService* sync_service) override {
    sync_service_observer_.Reset();
    profile_->RemoveUserData(kSignInPromoDataUploaderChecker);
  }

 private:
  const base::WeakPtr<content::WebContents> web_contents_;
  const base::WeakPtr<Profile> profile_;
  const syncer::DataType data_type_;
  base::OnceCallback<void(content::WebContents*)> move_callback_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observer_{this};
};

}  // namespace

namespace autofill {

AutofillBubbleSignInPromoController::AutofillBubbleSignInPromoController(
    content::WebContents& web_contents,
    signin_metrics::AccessPoint access_point,
    base::OnceCallback<void(content::WebContents*)> move_callback)
    : move_callback_(std::move(move_callback)),
      web_contents_(web_contents.GetWeakPtr()),
      access_point_(access_point) {}

AutofillBubbleSignInPromoController::~AutofillBubbleSignInPromoController() =
    default;

void AutofillBubbleSignInPromoController::OnSignInToChromeClicked(
    const AccountInfo& account) {
  // Signing in is triggered by the user interacting with the sign-in promo.
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  CHECK(switches::IsExplicitBrowserSigninUIOnDesktopEnabled());

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  signin_ui_util::SignInFromSingleAccountPromo(profile, account, access_point_);

  signin_util::SignedInState signed_in_state = signin_util::GetSignedInState(
      IdentityManagerFactory::GetForProfile(profile));

  // Make sure the access point is valid.
  CHECK(kAccessPointToDataTypeMap.contains(access_point_));
  syncer::DataType data_type = kAccessPointToDataTypeMap.at(access_point_);

  auto maybe_move_data = base::BindOnce(
      [](base::OnceCallback<void(content::WebContents*)> move_callback,
         syncer::DataType data_type, content::WebContents* web_contents) {
        Profile* profile =
            Profile::FromBrowserContext(web_contents->GetBrowserContext());
        profile->SetUserData(
            kSignInPromoDataUploaderChecker,
            std::make_unique<SignInPromoDataUploaderChecker>(
                web_contents, profile, data_type, std::move(move_callback)));
      },
      std::move(move_callback_), data_type);

  // If the sign in was already successful, move the data directly.
  if (signed_in_state == signin_util::SignedInState::kSignedIn) {
    std::move(maybe_move_data).Run(web_contents_.get());
    return;
  }

  // These states requires a sign in tab to be displayed. A tab helper attached
  // to the tab will take care of the move operation once signed in.
  if (signed_in_state != signin_util::SignedInState::kSignedOut &&
      signed_in_state != signin_util::SignedInState::kSignInPending) {
    return;
  }

  // TODO(crbug.com/319411636): Investigate how we could get the sign in tab
  // differently.
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
