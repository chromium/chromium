// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/save_card_bubble_controller_impl.h"

#include <stddef.h>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/strike_database_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/autofill/popup_constants.h"
#include "chrome/browser/ui/autofill/save_card_bubble_view.h"
#include "chrome/browser/ui/autofill/save_card_ui.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/strike_database.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_buildflags.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

SaveCardBubbleControllerImpl::SaveCardBubbleControllerImpl(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      web_contents_(web_contents),
      pref_service_(
          user_prefs::UserPrefs::Get(web_contents->GetBrowserContext())),
      weak_ptr_factory_(this) {
  security_state::SecurityInfo security_info;
  SecurityStateTabHelper::FromWebContents(web_contents)
      ->GetSecurityInfo(&security_info);
  security_level_ = security_info.security_level;
}

SaveCardBubbleControllerImpl::~SaveCardBubbleControllerImpl() {
  if (save_card_bubble_view_)
    save_card_bubble_view_->Hide();
}

void SaveCardBubbleControllerImpl::OfferLocalSave(
    const CreditCard& card,
    bool show_bubble,
    base::OnceClosure save_card_callback) {
  // Don't show the bubble if it's already visible.
  if (save_card_bubble_view_)
    return;

  is_upload_save_ = false;
  is_reshow_ = false;
  should_request_name_from_user_ = false;
  show_bubble_ = show_bubble;
  legal_message_lines_.clear();

  card_ = card;
  local_save_card_callback_ = std::move(save_card_callback);
  current_bubble_type_ = BubbleType::LOCAL_SAVE;
  if (show_bubble_) {
    ShowBubble();
    AutofillMetrics::LogSaveCardPromptMetric(
        AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, is_upload_save_,
        is_reshow_, should_request_name_from_user_,
        pref_service_->GetInteger(
            prefs::kAutofillAcceptSaveCreditCardPromptState),
        GetSecurityLevel());
  } else {
    ShowIconOnly();
  }
}

void SaveCardBubbleControllerImpl::OfferUploadSave(
    const CreditCard& card,
    std::unique_ptr<base::DictionaryValue> legal_message,
    bool should_request_name_from_user,
    bool show_bubble,
    base::OnceCallback<void(const base::string16&)> save_card_callback) {
  // Don't show the bubble if it's already visible.
  if (save_card_bubble_view_)
    return;

  // Fetch the logged-in user's AccountInfo if it has not yet been done.
  if (should_request_name_from_user && account_info_.IsEmpty())
    FetchAccountInfo();

  is_upload_save_ = true;
  is_reshow_ = false;
  should_request_name_from_user_ = should_request_name_from_user;
  show_bubble_ = show_bubble;
  if (show_bubble_) {
    // Can't move this into the other "if (show_bubble_)" below because an
    // invalid legal message would skip it.
    AutofillMetrics::LogSaveCardPromptMetric(
        AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, is_upload_save_,
        is_reshow_, should_request_name_from_user_,
        pref_service_->GetInteger(
            prefs::kAutofillAcceptSaveCreditCardPromptState),
        GetSecurityLevel());
  }

  if (!LegalMessageLine::Parse(*legal_message, &legal_message_lines_,
                               /*escape_apostrophes=*/true)) {
    AutofillMetrics::LogSaveCardPromptMetric(
        AutofillMetrics::SAVE_CARD_PROMPT_END_INVALID_LEGAL_MESSAGE,
        is_upload_save_, is_reshow_, should_request_name_from_user_,
        pref_service_->GetInteger(
            prefs::kAutofillAcceptSaveCreditCardPromptState),
        GetSecurityLevel());
    return;
  }

  card_ = card;
  upload_save_card_callback_ = std::move(save_card_callback);
  current_bubble_type_ = BubbleType::UPLOAD_SAVE;

  if (show_bubble_)
    ShowBubble();
  else
    ShowIconOnly();
}

void SaveCardBubbleControllerImpl::ShowBubbleForSignInPromo() {
  if (!ShouldShowSignInPromo())
    return;

  current_bubble_type_ = BubbleType::SIGN_IN_PROMO;

  // If DICe is disabled, then we need to know whether the user is signed in
  // to determine whether or not to show a sign-in vs sync promo.
  if (GetAccountInfo().IsEmpty())
    FetchAccountInfo();
  ShowBubble();
}

// Exists for testing purposes only.
void SaveCardBubbleControllerImpl::ShowBubbleForManageCardsForTesting(
    const CreditCard& card) {
  card_ = card;
  current_bubble_type_ = BubbleType::MANAGE_CARDS;
  ShowBubble();
}

void SaveCardBubbleControllerImpl::HideBubble() {
  if (save_card_bubble_view_) {
    save_card_bubble_view_->Hide();
    save_card_bubble_view_ = nullptr;
  }
}

void SaveCardBubbleControllerImpl::ReshowBubble() {
  // Don't show the bubble if it's already visible.
  if (save_card_bubble_view_)
    return;

  is_reshow_ = true;

  if (current_bubble_type_ == BubbleType::LOCAL_SAVE ||
      current_bubble_type_ == BubbleType::UPLOAD_SAVE) {
    AutofillMetrics::LogSaveCardPromptMetric(
        AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, is_upload_save_,
        is_reshow_, should_request_name_from_user_,
        pref_service_->GetInteger(
            prefs::kAutofillAcceptSaveCreditCardPromptState),
        GetSecurityLevel());
  }

  ShowBubble();
}

bool SaveCardBubbleControllerImpl::IsIconVisible() const {
  // If there is no bubble to show, then there should be no icon.
  return current_bubble_type_ != BubbleType::INACTIVE;
}

SaveCardBubbleView* SaveCardBubbleControllerImpl::save_card_bubble_view()
    const {
  return save_card_bubble_view_;
}

base::string16 SaveCardBubbleControllerImpl::GetWindowTitle() const {
  switch (current_bubble_type_) {
    case BubbleType::LOCAL_SAVE:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_LOCAL);
    case BubbleType::UPLOAD_SAVE:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_V3);
    case BubbleType::SIGN_IN_PROMO:
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      if (AccountConsistencyModeManager::IsDiceEnabledForProfile(
              GetProfile())) {
        return l10n_util::GetStringUTF16(IDS_AUTOFILL_SYNC_PROMO_MESSAGE);
      }
#endif
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_CARD_SAVED);
    case BubbleType::MANAGE_CARDS:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_CARD_SAVED);
    case BubbleType::INACTIVE:
      NOTREACHED();
      return base::string16();
  }
}

base::string16 SaveCardBubbleControllerImpl::GetExplanatoryMessage() const {
  if (current_bubble_type_ != BubbleType::UPLOAD_SAVE)
    return base::string16();

  bool offer_to_save_on_device_message =
      OfferStoreUnmaskedCards() &&
      !IsAutofillNoLocalSaveOnUploadSuccessExperimentEnabled();
  if (should_request_name_from_user_) {
    return l10n_util::GetStringUTF16(
        offer_to_save_on_device_message
            ? IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_V3_WITH_NAME_AND_DEVICE
            : IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_V3_WITH_NAME);
  }
  return l10n_util::GetStringUTF16(
      offer_to_save_on_device_message
          ? IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_V3_WITH_DEVICE
          : IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_V3);
}

const AccountInfo& SaveCardBubbleControllerImpl::GetAccountInfo() const {
  return account_info_;
}

Profile* SaveCardBubbleControllerImpl::GetProfile() const {
  if (!web_contents())
    return nullptr;
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

const CreditCard& SaveCardBubbleControllerImpl::GetCard() const {
  return card_;
}

bool SaveCardBubbleControllerImpl::ShouldRequestNameFromUser() const {
  return should_request_name_from_user_;
}

bool SaveCardBubbleControllerImpl::ShouldShowSignInPromo() const {
  if (is_upload_save_ || !base::FeatureList::IsEnabled(
                             features::kAutofillSaveCardSignInAfterLocalSave))
    return false;

  const browser_sync::ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(GetProfile());

  return !sync_service ||
         sync_service->HasDisableReason(
             browser_sync::ProfileSyncService::DISABLE_REASON_NOT_SIGNED_IN) ||
         sync_service->HasDisableReason(
             browser_sync::ProfileSyncService::DISABLE_REASON_USER_CHOICE);
}

bool SaveCardBubbleControllerImpl::CanAnimate() const {
  return can_animate_;
}

void SaveCardBubbleControllerImpl::OnSyncPromoAccepted(
    const AccountInfo& account,
    signin_metrics::AccessPoint access_point,
    bool is_default_promo_account) {
  DCHECK(current_bubble_type_ == BubbleType::SIGN_IN_PROMO ||
         current_bubble_type_ == BubbleType::MANAGE_CARDS);
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  signin_ui_util::EnableSyncFromPromo(browser, account, access_point,
                                      is_default_promo_account);
}

void SaveCardBubbleControllerImpl::OnSaveButton(
    const base::string16& cardholder_name) {
  save_card_bubble_view_ = nullptr;

  switch (current_bubble_type_) {
    case BubbleType::UPLOAD_SAVE: {
      DCHECK(!upload_save_card_callback_.is_null());

      base::string16 name_provided_by_user;
      if (!cardholder_name.empty()) {
        // Log whether the name was changed by the user or simply accepted
        // without edits.
        AutofillMetrics::LogSaveCardCardholderNameWasEdited(
            cardholder_name != base::UTF8ToUTF16(account_info_.full_name));
        // Trim the cardholder name provided by the user and send it in the
        // callback so it can be included in the final request.
        DCHECK(ShouldRequestNameFromUser());
        base::TrimWhitespace(cardholder_name, base::TRIM_ALL,
                             &name_provided_by_user);
      }
      std::move(upload_save_card_callback_).Run(name_provided_by_user);
      break;
    }
    case BubbleType::LOCAL_SAVE:
      DCHECK(!local_save_card_callback_.is_null());
      // Show an animated card saved confirmation message next time
      // UpdateIcon() is called.
      can_animate_ = base::FeatureList::IsEnabled(
          features::kAutofillSaveCardSignInAfterLocalSave);

      std::move(local_save_card_callback_).Run();
      break;
    case BubbleType::MANAGE_CARDS:
      AutofillMetrics::LogManageCardsPromptMetric(
          AutofillMetrics::MANAGE_CARDS_DONE, is_upload_save_);
      return;
    case BubbleType::SIGN_IN_PROMO:
    case BubbleType::INACTIVE:
      NOTREACHED();
  }

  const BubbleType previous_bubble_type = current_bubble_type_;
  current_bubble_type_ = BubbleType::INACTIVE;

  // If user just saved a card locally, the next bubble can either be a sign-in
  // promo or a manage cards view. If we need to show a sign-in promo, that
  // will be handled by OnAnimationEnded(), otherwise clicking the icon again
  // will show the MANAGE_CARDS bubble, which is set here.
  if (previous_bubble_type == BubbleType::LOCAL_SAVE &&
      base::FeatureList::IsEnabled(
          features::kAutofillSaveCardSignInAfterLocalSave)) {
    current_bubble_type_ = BubbleType::MANAGE_CARDS;
  }

  if (previous_bubble_type == BubbleType::LOCAL_SAVE ||
      previous_bubble_type == BubbleType::UPLOAD_SAVE) {
    AutofillMetrics::LogSaveCardPromptMetric(
        AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, is_upload_save_,
        is_reshow_, should_request_name_from_user_,
        pref_service_->GetInteger(
            prefs::kAutofillAcceptSaveCreditCardPromptState),
        GetSecurityLevel());
    pref_service_->SetInteger(
        prefs::kAutofillAcceptSaveCreditCardPromptState,
        prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_ACCEPTED);
  }
}

void SaveCardBubbleControllerImpl::OnCancelButton() {
  // Should only be applicable for non-material UI,
  // as Harmony dialogs do not have [No thanks] buttons.
  const BubbleType previous_bubble_type = current_bubble_type_;
  current_bubble_type_ = BubbleType::INACTIVE;
  upload_save_card_callback_.Reset();
  local_save_card_callback_.Reset();

  if (previous_bubble_type == BubbleType::LOCAL_SAVE ||
      previous_bubble_type == BubbleType::UPLOAD_SAVE) {
    AutofillMetrics::LogSaveCardPromptMetric(
        AutofillMetrics::SAVE_CARD_PROMPT_END_DENIED, is_upload_save_,
        is_reshow_, should_request_name_from_user_,
        pref_service_->GetInteger(
            prefs::kAutofillAcceptSaveCreditCardPromptState),
        GetSecurityLevel());
    pref_service_->SetInteger(
        prefs::kAutofillAcceptSaveCreditCardPromptState,
        prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_DENIED);
    if (show_bubble_ &&
        base::FeatureList::IsEnabled(
            features::kAutofillSaveCreditCardUsesStrikeSystem)) {
      // If save was cancelled and the bubble was actually shown (NOT just the
      // icon), count that as a strike against offering save in the future.
      StrikeDatabase* strike_database = GetStrikeDatabase();
      strike_database->AddStrike(
          strike_database->GetKeyForCreditCardSave(
              base::UTF16ToUTF8(card_.LastFourDigits())),
          base::BindRepeating(
              &SaveCardBubbleControllerImpl::OnStrikeChangeComplete,
              weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void SaveCardBubbleControllerImpl::OnLegalMessageLinkClicked(const GURL& url) {
  OpenUrl(url);
  AutofillMetrics::LogSaveCardPromptMetric(
      AutofillMetrics::SAVE_CARD_PROMPT_DISMISS_CLICK_LEGAL_MESSAGE,
      is_upload_save_, is_reshow_, should_request_name_from_user_,
      pref_service_->GetInteger(
          prefs::kAutofillAcceptSaveCreditCardPromptState),
      GetSecurityLevel());
}

void SaveCardBubbleControllerImpl::OnManageCardsClicked() {
  DCHECK(current_bubble_type_ == BubbleType::MANAGE_CARDS);

  AutofillMetrics::LogManageCardsPromptMetric(
      AutofillMetrics::MANAGE_CARDS_MANAGE_CARDS, is_upload_save_);

  ShowPaymentsSettingsPage();
}

void SaveCardBubbleControllerImpl::ShowPaymentsSettingsPage() {
  chrome::ShowSettingsSubPage(
      chrome::FindBrowserWithWebContents(web_contents()),
      chrome::kPaymentsSubPage);
}

void SaveCardBubbleControllerImpl::OnBubbleClosed() {
  save_card_bubble_view_ = nullptr;
  // Sign-in promo should only be shown once, so if it was displayed presently,
  // reopening the bubble will show the card management bubble.
  if (current_bubble_type_ == BubbleType::SIGN_IN_PROMO)
    current_bubble_type_ = BubbleType::MANAGE_CARDS;
  UpdateIcon();
  if (observer_for_testing_)
    observer_for_testing_->OnBubbleClosed();
}

void SaveCardBubbleControllerImpl::OnAnimationEnded() {
  // Do not repeat the animation next time UpdateIcon() is called, unless
  // explicitly set somewhere else.
  can_animate_ = false;

  // We do not want to show the promo if the user clicked on the icon and the
  // manage cards bubble started to show.
  if (!save_card_bubble_view_)
    ShowBubbleForSignInPromo();
}

const LegalMessageLines& SaveCardBubbleControllerImpl::GetLegalMessageLines()
    const {
  return legal_message_lines_;
}

bool SaveCardBubbleControllerImpl::IsUploadSave() const {
  return is_upload_save_;
}

BubbleType SaveCardBubbleControllerImpl::GetBubbleType() const {
  return current_bubble_type_;
}

void SaveCardBubbleControllerImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() || !navigation_handle->HasCommitted())
    return;

  // Nothing to do if there's no bubble available.
  if (current_bubble_type_ == BubbleType::INACTIVE)
    return;

  // Don't react to same-document (fragment) navigations.
  if (navigation_handle->IsSameDocument())
    return;

  // Don't do anything if a navigation occurs before a user could reasonably
  // interact with the bubble.
  const base::TimeDelta elapsed_time =
      AutofillClock::Now() - bubble_shown_timestamp_;
  if (elapsed_time < kCardBubbleSurviveNavigationTime)
    return;

  // Otherwise, get rid of the bubble and icon.
  const BubbleType previous_bubble_type = current_bubble_type_;
  current_bubble_type_ = BubbleType::INACTIVE;

  upload_save_card_callback_.Reset();
  local_save_card_callback_.Reset();
  bool bubble_was_visible = save_card_bubble_view_;
  if (bubble_was_visible) {
    save_card_bubble_view_->Hide();
    OnBubbleClosed();
  } else {
    UpdateIcon();
  }

  if (previous_bubble_type == BubbleType::LOCAL_SAVE ||
      previous_bubble_type == BubbleType::UPLOAD_SAVE) {
    AutofillMetrics::LogSaveCardPromptMetric(
        bubble_was_visible
            ? AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_SHOWING
            : AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_HIDDEN,
        is_upload_save_, is_reshow_, should_request_name_from_user_,
        pref_service_->GetInteger(
            prefs::kAutofillAcceptSaveCreditCardPromptState),
        GetSecurityLevel());
    if (base::FeatureList::IsEnabled(
            features::kAutofillSaveCreditCardUsesStrikeSystem) &&
        show_bubble_) {
      // If the save offer was ignored and the bubble was actually shown (NOT
      // just the icon), count that as a strike against offering save in the
      // future.
      StrikeDatabase* strike_database = GetStrikeDatabase();
      strike_database->AddStrike(
          strike_database->GetKeyForCreditCardSave(
              base::UTF16ToUTF8(card_.LastFourDigits())),
          base::BindRepeating(
              &SaveCardBubbleControllerImpl::OnStrikeChangeComplete,
              weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void SaveCardBubbleControllerImpl::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN)
    HideBubble();
}

void SaveCardBubbleControllerImpl::WebContentsDestroyed() {
  HideBubble();
}

void SaveCardBubbleControllerImpl::FetchAccountInfo() {
  Profile* profile = GetProfile();
  if (!profile)
    return;
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  AccountTrackerService* account_tracker =
      AccountTrackerServiceFactory::GetForProfile(profile);
  if (!signin_manager || !account_tracker)
    return;
  account_info_ = account_tracker->GetAccountInfo(
      signin_manager->GetAuthenticatedAccountId());
}

StrikeDatabase* SaveCardBubbleControllerImpl::GetStrikeDatabase() {
  Profile* profile = GetProfile();
  // No need to return a StrikeDatabase in incognito mode. We don't allow saving
  // of Autofill data while in incognito, so an incognito code path should never
  // get this far.
  DCHECK(profile && !profile->IsOffTheRecord());
  return StrikeDatabaseFactory::GetForProfile(profile);
}

void SaveCardBubbleControllerImpl::ShowBubble() {
  DCHECK(current_bubble_type_ != BubbleType::INACTIVE);
  // Upload save callback should not be null for UPLOAD_SAVE state.
  DCHECK(!(upload_save_card_callback_.is_null() &&
           current_bubble_type_ == BubbleType::UPLOAD_SAVE));
  // Local save callback should not be null for LOCAL_SAVE state.
  DCHECK(!(local_save_card_callback_.is_null() &&
           current_bubble_type_ == BubbleType::LOCAL_SAVE));
  DCHECK(!save_card_bubble_view_);

  // Need to create location bar icon before bubble, otherwise bubble will be
  // unanchored.
  UpdateIcon();

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  save_card_bubble_view_ = browser->window()->ShowSaveCreditCardBubble(
      web_contents(), this, is_reshow_);
  DCHECK(save_card_bubble_view_);

  // Update icon after creating |save_card_bubble_view_| so that icon will show
  // its "toggled on" state.
  UpdateIcon();

  bubble_shown_timestamp_ = AutofillClock::Now();

  switch (current_bubble_type_) {
    case BubbleType::UPLOAD_SAVE:
    case BubbleType::LOCAL_SAVE:
      AutofillMetrics::LogSaveCardPromptMetric(
          AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, is_upload_save_, is_reshow_,
          should_request_name_from_user_,
          pref_service_->GetInteger(
              prefs::kAutofillAcceptSaveCreditCardPromptState),
          GetSecurityLevel());
      break;
    case BubbleType::MANAGE_CARDS:
      AutofillMetrics::LogManageCardsPromptMetric(
          AutofillMetrics::MANAGE_CARDS_SHOWN, is_upload_save_);
      break;
    case BubbleType::SIGN_IN_PROMO:
      break;
    case BubbleType::INACTIVE:
      NOTREACHED();
  }

  if (observer_for_testing_) {
    observer_for_testing_->OnBubbleShown();
  }
}

void SaveCardBubbleControllerImpl::ShowIconOnly() {
  DCHECK(current_bubble_type_ != BubbleType::INACTIVE);
  // Upload save callback should not be null for UPLOAD_SAVE state.
  DCHECK(!(upload_save_card_callback_.is_null() &&
           current_bubble_type_ == BubbleType::UPLOAD_SAVE));
  // Local save callback should not be null for LOCAL_SAVE state.
  DCHECK(!(local_save_card_callback_.is_null() &&
           current_bubble_type_ == BubbleType::LOCAL_SAVE));
  DCHECK(!save_card_bubble_view_);

  // Show the icon only. The bubble can still be displayed if the user
  // explicitly clicks the icon.
  UpdateIcon();

  bubble_shown_timestamp_ = AutofillClock::Now();

  switch (current_bubble_type_) {
    case BubbleType::UPLOAD_SAVE:
    case BubbleType::LOCAL_SAVE:
      // TODO(crbug/884817): Log metrics for "bubble not shown".
      break;
    case BubbleType::MANAGE_CARDS:
    case BubbleType::SIGN_IN_PROMO:
    case BubbleType::INACTIVE:
      NOTREACHED();
  }
}

void SaveCardBubbleControllerImpl::UpdateIcon() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!browser)
    return;
  LocationBar* location_bar = browser->window()->GetLocationBar();
  location_bar->UpdateSaveCreditCardIcon();
}

void SaveCardBubbleControllerImpl::OpenUrl(const GURL& url) {
  web_contents()->OpenURL(content::OpenURLParams(
      url, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false));
}

void SaveCardBubbleControllerImpl::OnStrikeChangeComplete(
    const int num_strikes) {
  if (observer_for_testing_)
    observer_for_testing_->OnSCBCStrikeChangeComplete();
}

security_state::SecurityLevel SaveCardBubbleControllerImpl::GetSecurityLevel()
    const {
  return security_level_;
}

}  // namespace autofill
