// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/login_ui_service.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/url_constants.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/signin_header_helper.h"

#if !defined(OS_CHROMEOS)
#include "base/scoped_observer.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/sync/base/sync_prefs.h"
#include "components/unified_consent/feature.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service.h"

// The sync consent bump is shown after startup when a profile's browser
// instance becomes active or when there is already an active instance.
// It is only shown when |ShouldShowConsentBumpFor(profile)| returns true for a
// given profile |profile|.
class ConsentBumpActivator : public BrowserListObserver,
                             public LoginUIService::Observer,
                             public OAuth2TokenService::Observer {
 public:
  // Creates a ConsentBumpActivator for |profile| which is owned by
  // |login_ui_service|.
  ConsentBumpActivator(LoginUIService* login_ui_service, Profile* profile)
      : login_ui_service_(login_ui_service),
        profile_(profile),
        scoped_browser_list_observer_(this),
        scoped_login_ui_service_observer_(this),
        scoped_token_service_observer_(this) {
    ProfileOAuth2TokenService* token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
    if (token_service->AreAllCredentialsLoaded())
      OnRefreshTokensLoaded();
    else
      scoped_token_service_observer_.Add(token_service);
  }

  // OAuth2TokenService::Observer:
  void OnRefreshTokensLoaded() override {
    scoped_token_service_observer_.RemoveAll();

    SigninManager* signin_manager =
        SigninManagerFactory::GetForProfile(profile_);
    ProfileOAuth2TokenService* token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);

    // Avoid showing the consent bump if the refresh token is missing or is in
    // an permanent auth error state. When the tokens are loaded, this
    // corresponds to the case when the refresh token was invalidated
    // client-side after the user signed out of a Google website (e.g. the user
    // signed out of Gmail).
    if (token_service->RefreshTokenHasError(
            signin_manager->GetAuthenticatedAccountId())) {
      unified_consent::UnifiedConsentService* consent_service =
          UnifiedConsentServiceFactory::GetForProfile(profile_);
      if (consent_service->ShouldShowConsentBump()) {
        consent_service->RecordConsentBumpSuppressReason(
            unified_consent::metrics::ConsentBumpSuppressReason::kSyncPaused);
      }
      return;
    }

    // Check if there is already an active browser window for |profile|.
    Browser* active_browser = chrome::FindLastActiveWithProfile(profile_);
    if (active_browser)
      OnBrowserSetLastActive(active_browser);
    else
      scoped_browser_list_observer_.Add(BrowserList::GetInstance());
  }

  // BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override {
    if (browser->profile() != profile_)
      return;
    // We only try to show the consent bump once after startup, so remove |this|
    // as a |BrowserListObserver|.
    scoped_browser_list_observer_.RemoveAll();

    if (ShouldShowConsentBumpFor(profile_)) {
      selected_browser_ = browser;
      scoped_login_ui_service_observer_.Add(login_ui_service_);
      selected_browser_->signin_view_controller()->ShowModalSyncConsentBump(
          selected_browser_);
    }
  }

  // LoginUIService::Observer:
  void OnSyncConfirmationUIClosed(
      LoginUIService::SyncConfirmationUIClosedResult result) override {
    scoped_login_ui_service_observer_.RemoveAll();

    unified_consent::UnifiedConsentService* consent_service =
        UnifiedConsentServiceFactory::GetForProfile(profile_);

    consent_service->MarkConsentBumpShown();

    switch (result) {
      case LoginUIService::CONFIGURE_SYNC_FIRST:
        // Opt into Unity and open settings page to make configurations.
        consent_service->SetUnifiedConsentGiven(true);
        chrome::ShowSettingsSubPage(selected_browser_,
                                    chrome::kSyncSetupSubPage);
        break;
      case LoginUIService::SYNC_WITH_DEFAULT_SETTINGS:
        // Opt into Unity with default configuration.
        consent_service->SetUnifiedConsentGiven(true);
        break;
      case LoginUIService::ABORT_SIGNIN:
        // "Make no changes" was selected.
        DCHECK(!consent_service->IsUnifiedConsentGiven());
        break;
    }
  }

  // This should only be called after the browser has been set up, otherwise
  // this might crash because the profile has not been fully initialized yet.
  static bool ShouldShowConsentBumpFor(Profile* profile) {
    if (!profile->IsSyncAllowed() ||
        !unified_consent::IsUnifiedConsentFeatureWithBumpEnabled() ||
        !ProfileSyncServiceFactory::HasProfileSyncService(profile))
      return false;

    unified_consent::UnifiedConsentService* consent_service =
        UnifiedConsentServiceFactory::GetForProfile(profile);
    if (!consent_service->ShouldShowConsentBump())
      return false;

    sync_ui_util::MessageType sync_status = sync_ui_util::GetStatus(
        profile, ProfileSyncServiceFactory::GetForProfile(profile),
        *SigninManagerFactory::GetForProfile(profile));

    return sync_status == sync_ui_util::SYNCED;
  }

 private:
  LoginUIService* login_ui_service_;  // owner

  Profile* profile_;

  ScopedObserver<BrowserList, ConsentBumpActivator>
      scoped_browser_list_observer_;
  ScopedObserver<LoginUIService, ConsentBumpActivator>
      scoped_login_ui_service_observer_;
  ScopedObserver<OAuth2TokenService, ConsentBumpActivator>
      scoped_token_service_observer_;

  // Used for the action handling of the consent bump.
  Browser* selected_browser_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ConsentBumpActivator);
};

#endif  // !defined(OS_CHROMEOS)

LoginUIService::LoginUIService(Profile* profile)
#if !defined(OS_CHROMEOS)
    : profile_(profile)
#endif
{
#if !defined(OS_CHROMEOS)
  if (unified_consent::IsUnifiedConsentFeatureWithBumpEnabled()) {
    consent_bump_activator_ =
        std::make_unique<ConsentBumpActivator>(this, profile);
  }
#endif
}

LoginUIService::~LoginUIService() {}

void LoginUIService::AddObserver(LoginUIService::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void LoginUIService::RemoveObserver(LoginUIService::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

LoginUIService::LoginUI* LoginUIService::current_login_ui() const {
  return ui_list_.empty() ? nullptr : ui_list_.front();
}

void LoginUIService::SetLoginUI(LoginUI* ui) {
  ui_list_.remove(ui);
  ui_list_.push_front(ui);
}

void LoginUIService::LoginUIClosed(LoginUI* ui) {
  ui_list_.remove(ui);
}

void LoginUIService::SyncConfirmationUIClosed(
    SyncConfirmationUIClosedResult result) {
  for (Observer& observer : observer_list_)
    observer.OnSyncConfirmationUIClosed(result);
}

void LoginUIService::ShowLoginPopup() {
#if defined(OS_CHROMEOS)
  NOTREACHED();
#else
  // There is no sign-in flow for guest or system profile.
  if (profile_->IsGuestSession() || profile_->IsSystemProfile())
    return;
  // Locked profile should be unlocked with UserManager only.
  ProfileAttributesEntry* entry;
  if (g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_->GetPath(), &entry) &&
      entry->IsSigninRequired()) {
    return;
  }

  chrome::ScopedTabbedBrowserDisplayer displayer(profile_);
  chrome::ShowBrowserSignin(
      displayer.browser(),
      signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS);
#endif
}

void LoginUIService::DisplayLoginResult(Browser* browser,
                                        const base::string16& error_message,
                                        const base::string16& email) {
#if defined(OS_CHROMEOS)
  // ChromeOS doesn't have the avatar bubble so it never calls this function.
  NOTREACHED();
#else
  is_displaying_profile_blocking_error_message_ = false;
  last_login_result_ = error_message;
  last_login_error_email_ = email;
  if (!error_message.empty()) {
    if (browser)
      browser->signin_view_controller()->ShowModalSigninErrorDialog(browser);
    else
      UserManagerProfileDialog::DisplayErrorMessage();
  } else if (browser) {
    browser->window()->ShowAvatarBubbleFromAvatarButton(
        error_message.empty() ? BrowserWindow::AVATAR_BUBBLE_MODE_CONFIRM_SIGNIN
                              : BrowserWindow::AVATAR_BUBBLE_MODE_SHOW_ERROR,
        signin::ManageAccountsParams(),
        signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS, false);
  }
#endif
}

void LoginUIService::SetProfileBlockingErrorMessage() {
  last_login_result_ = base::string16();
  last_login_error_email_ = base::string16();
  is_displaying_profile_blocking_error_message_ = true;
}

bool LoginUIService::IsDisplayingProfileBlockedErrorMessage() const {
  return is_displaying_profile_blocking_error_message_;
}

const base::string16& LoginUIService::GetLastLoginResult() const {
  return last_login_result_;
}

const base::string16& LoginUIService::GetLastLoginErrorEmail() const {
  return last_login_error_email_;
}
