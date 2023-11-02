// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_creation_signed_in_flow_controller.h"

#include "base/trace_event/trace_event.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/profiles/profile_customization_bubble_sync_controller.h"
#include "chrome/browser/ui/views/profiles/profile_customization_bubble_view.h"

namespace {

// Shows the customization bubble if possible. The bubble won't be shown if the
// color is enforced by policy or downloaded through Sync or the default theme
// should be used. An IPH is shown after the bubble, or right away if the bubble
// cannot be shown.
void ShowCustomizationBubble(absl::optional<SkColor> new_profile_color,
                             Browser* browser) {
  DCHECK(browser);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view || !browser_view->toolbar_button_provider())
    return;
  views::View* anchor_view =
      browser_view->toolbar_button_provider()->GetAvatarToolbarButton();
  CHECK(anchor_view);

  if (ProfileCustomizationBubbleSyncController::CanThemeSyncStart(
          browser->profile())) {
    // For sync users, their profile color has not been applied yet. Call a
    // helper class that applies the color and shows the bubble only if there is
    // no conflict with a synced theme / color.
    ProfileCustomizationBubbleSyncController::
        ApplyColorAndShowBubbleWhenNoValueSynced(
            browser, anchor_view,
            /*suggested_profile_color=*/new_profile_color.value());
  } else {
    // For non syncing users, simply show the bubble.
    ProfileCustomizationBubbleView::CreateBubble(browser, anchor_view);
  }
}

void MaybeShowProfileSwitchIPH(Browser* browser) {
  DCHECK(browser);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view)
    return;
  browser_view->MaybeShowProfileSwitchIPH();
}

void ContinueSAMLSignin(std::unique_ptr<content::WebContents> saml_wc,
                        Browser* browser) {
  DCHECK(browser);
  browser->tab_strip_model()->ReplaceWebContentsAt(0, std::move(saml_wc));

  ProfileMetrics::LogProfileAddSignInFlowOutcome(
      ProfileMetrics::ProfileSignedInFlowOutcome::kSAML);
}

}  // namespace

ProfileCreationSignedInFlowController::ProfileCreationSignedInFlowController(
    ProfilePickerWebContentsHost* host,
    Profile* profile,
    std::unique_ptr<content::WebContents> contents,
    absl::optional<SkColor> profile_color,
    bool is_saml)
    : ProfilePickerSignedInFlowController(host,
                                          profile,
                                          std::move(contents),
                                          profile_color),
      is_saml_(is_saml) {}

ProfileCreationSignedInFlowController::
    ~ProfileCreationSignedInFlowController() {
  // Record unfinished signed-in profile creation.
  if (!is_finished_) {
    // Schedule the profile for deletion, it's not needed any more.
    g_browser_process->profile_manager()->ScheduleEphemeralProfileForDeletion(
        profile()->GetPath());

    // TODO(crbug.com/1300109): Consider moving this recording into
    // ProfilePickerTurnSyncOnDelegate and unify this code with Cancel().
    ProfileMetrics::LogProfileAddSignInFlowOutcome(
        ProfileMetrics::ProfileSignedInFlowOutcome::kAbortedAfterSignIn);
  }
}

void ProfileCreationSignedInFlowController::Init() {
  // TODO(crbug.com/1300109): Separate the SAML case into a subclass of
  // ProfileCreationSignedInFlowController to streamline the code.
  if (is_saml_) {
    FinishAndOpenBrowserForSAML();
    return;
  }

  // Stop with the sign-in navigation and show a spinner instead. The spinner
  // will be shown until TurnSyncOnHelper figures out whether it's a
  // managed account and whether sync is disabled by policies (which in some
  // cases involves fetching policies and can take a couple of seconds).
  host()->ShowScreen(contents(), GetSyncConfirmationURL(/*loading=*/true));

  ProfilePickerSignedInFlowController::Init();

  // Listen for extended account info getting fetched.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  profile_name_resolver_ =
      std::make_unique<ProfileNameResolver>(identity_manager);
}

void ProfileCreationSignedInFlowController::Cancel() {
  if (is_finished_)
    return;

  is_finished_ = true;

  // Schedule the profile for deletion, it's not needed any more.
  g_browser_process->profile_manager()->ScheduleEphemeralProfileForDeletion(
      profile()->GetPath());
}

void ProfileCreationSignedInFlowController::FinishAndOpenBrowser(
    ProfilePicker::BrowserOpenedCallback callback) {
  // Do nothing if the sign-in flow is aborted or if this has already been
  // called. Note that this can get called first time from a special case
  // handling (such as the Settings link) and than second time when the
  // TurnSyncOnHelper finishes.
  if (is_finished_)
    return;
  is_finished_ = true;

  if (profile_name_resolver_->resolved_profile_name().empty()) {
    // Delay finishing the flow until we have obtained a profile name.
    profile_name_resolver_->set_on_profile_name_resolved_callback(
        base::BindOnce(
            &ProfileCreationSignedInFlowController::FinishAndOpenBrowserImpl,
            // Unretained ok: `this` outlives `profile_name_resolver_`.
            base::Unretained(this), std::move(callback)));
  } else {
    FinishAndOpenBrowserImpl(std::move(callback));
  }
}

void ProfileCreationSignedInFlowController::FinishAndOpenBrowserImpl(
    ProfilePicker::BrowserOpenedCallback callback) {
  TRACE_EVENT1(
      "browser",
      "ProfileCreationSignedInFlowController::FinishAndOpenBrowserImpl",
      "profile_path", profile()->GetPath().AsUTF8Unsafe());
  std::u16string name_for_signed_in_profile =
      profile_name_resolver_->resolved_profile_name();
  profile_name_resolver_.reset();
  DCHECK(!name_for_signed_in_profile.empty());

  FinalizeNewProfileSetup(profile(), name_for_signed_in_profile);

  ProfileMetrics::LogProfileAddNewUser(
      ProfileMetrics::ADD_NEW_PROFILE_PICKER_SIGNED_IN);

  // If there's no custom callback specified (that overrides profile
  // customization bubble), Chrome should show the customization bubble.
  if (!callback) {
    // If there's no color to apply to the profile, skip the customization
    // bubble and trigger an IPH, instead.
    if (ThemeServiceFactory::GetForProfile(profile())->UsingPolicyTheme() ||
        !GetProfileColor().has_value()) {
      callback = base::BindOnce(&MaybeShowProfileSwitchIPH);
    } else {
      callback = base::BindOnce(&ShowCustomizationBubble, GetProfileColor());

      // If sync cannot start, we apply `GetProfileColor()` right away before
      // opening a browser window to avoid flicker. Otherwise, it's applied
      // later by code triggered from ShowCustomizationBubble().
      if (!ProfileCustomizationBubbleSyncController::CanThemeSyncStart(
              profile())) {
        auto* theme_service = ThemeServiceFactory::GetForProfile(profile());
        theme_service->BuildAutogeneratedThemeFromColor(
            GetProfileColor().value());
      }
    }
  }

  ExitPickerAndRunInNewBrowser(std::move(callback));
}

void ProfileCreationSignedInFlowController::ExitPickerAndRunInNewBrowser(
    ProfilePicker::BrowserOpenedCallback callback) {
  profiles::OpenBrowserWindowForProfile(
      base::BindOnce(&ProfileCreationSignedInFlowController::OnBrowserOpened,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      /*always_create=*/false,   // Don't create a window if one already exists.
      /*is_new_profile=*/false,  // Don't create a first run window.
      /*unblock_extensions=*/false,  // There is no need to unblock all
                                     // extensions because we only open browser
                                     // window if the Profile is not locked.
                                     // Hence there is no extension blocked.
      profile());
}

void ProfileCreationSignedInFlowController::FinishAndOpenBrowserForSAML() {
  // First, free up `contents()` to be moved to a new browser window.
  host()->ShowScreenInPickerContents(
      GURL(url::kAboutBlankURL),
      /*navigation_finished_closure=*/
      base::BindOnce(
          &ProfileCreationSignedInFlowController::OnSignInContentsFreedUp,
          // Unretained is enough as the callback is called by a
          // member of `host_` that outlives `this`.
          base::Unretained(this)));
}

void ProfileCreationSignedInFlowController::OnSignInContentsFreedUp() {
  DCHECK(!is_finished_);
  is_finished_ = true;

  DCHECK(!profile_name_resolver_);
  contents()->SetDelegate(nullptr);

  FinalizeNewProfileSetup(profile(),
                          profiles::GetDefaultNameForNewEnterpriseProfile());
  ProfileMetrics::LogProfileAddNewUser(
      ProfileMetrics::ADD_NEW_PROFILE_PICKER_SIGNED_IN);

  ExitPickerAndRunInNewBrowser(
      base::BindOnce(&ContinueSAMLSignin, ReleaseContents()));
}

void ProfileCreationSignedInFlowController::OnBrowserOpened(
    ProfilePicker::BrowserOpenedCallback finish_flow_callback,
    Profile* profile_with_browser_opened) {
  CHECK_EQ(profile_with_browser_opened, profile());
  TRACE_EVENT1("browser",
               "ProfileCreationSignedInFlowController::OnBrowserOpened",
               "profile_path", profile()->GetPath().AsUTF8Unsafe());

  // Hide the flow window. This posts a task on the message loop to destroy the
  // window incl. this view.
  host()->Clear();

  if (!finish_flow_callback)
    return;

  Browser* browser = chrome::FindLastActiveWithProfile(profile());
  CHECK(browser);
  std::move(finish_flow_callback).Run(browser);
}
