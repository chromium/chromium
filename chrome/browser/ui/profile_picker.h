// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROFILE_PICKER_H_
#define CHROME_BROWSER_UI_PROFILE_PICKER_H_

#include "base/callback_forward.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/signin/enterprise_profile_welcome_ui.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

class GURL;
class Profile;
namespace content {
class BrowserContext;
}

namespace views {
class View;
class WebView;
}  // namespace views

class ProfilePicker {
 public:
  // Only work when passed as the argument 'on_select_profile_target_url' to
  // ProfilePicker::Show.
  static const char kTaskManagerUrl[];

  // An entry point that triggers the profile picker window to open.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class EntryPoint {
    kOnStartup = 0,
    kProfileMenuManageProfiles = 1,
    kProfileMenuAddNewProfile = 2,
    kOpenNewWindowAfterProfileDeletion = 3,
    // A new session was sarted while Chrome was already running (e.g. by
    // clicking on the tray icon on Windows).
    kNewSessionOnExistingProcess = 4,
    kProfileLocked = 5,
    kUnableToCreateBrowser = 6,
    kBackgroundModeManager = 7,
    // May only be used on lacros, opens an account picker, listing all accounts
    // that are not used in the provided profile, yet.
    kLacrosSelectAvailableAccount = 8,
    kMaxValue = kLacrosSelectAvailableAccount,
  };

  // Values for the ProfilePickerOnStartupAvailability policy. Should not be
  // re-numbered. See components/policy/resources/policy_templates.json for
  // documentation.
  enum class AvailabilityOnStartup {
    kEnabled = 0,
    kDisabled = 1,
    kForced = 2,
    kMax = kForced
  };

  ProfilePicker(const ProfilePicker&) = delete;
  ProfilePicker& operator=(const ProfilePicker&) = delete;

  // Shows the Profile picker for the given `entry_point` or re-activates an
  // existing one (if `custom_profile_path` matches the current one) or hides
  // the current window and shows a new one (if `custom_profile_path` does not
  // match the current one). When reactivated, the displayed page is not
  // updated. `custom_profile_path` specifies the profile that should be used to
  // render the profile picker and may be non-empty only for the
  // `kLacrosSelectAvailableAccount` entry point.
  // TODO(crbug.com/1226076): Clean the API up, make the optional params part of
  // a struct together with EntryPoint.
  static void Show(
      EntryPoint entry_point,
      const GURL& on_select_profile_target_url = GURL(),
      const base::FilePath& custom_profile_path = base::FilePath());

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Starts the Dice sign-in flow. The layout of the window gets updated for the
  // sign-in flow. At the same time, the new profile is created and the sign-in
  // page is rendered using the new profile.
  // The new profile uses a theme generated from `profile_color` if provided or
  // the default theme.
  // `switch_finished_callback` gets informed whether the creation of the new
  // profile succeeded and the sign-in page gets displayed.
  static void SwitchToDiceSignIn(
      absl::optional<SkColor> profile_color,
      base::OnceCallback<void(bool)> switch_finished_callback);
#endif

  // Starts the flow to set-up a signed-in profile. `signed_in_profile` must
  // have an unconsented primary account.
  static void SwitchToSignedInFlow(absl::optional<SkColor> profile_color,
                                   Profile* signed_in_profile);

  // Cancel the signed-in flow and returns back to the main picker screen (if
  // the original EntryPoint was to open the picker). Must only be called from
  // within the signed-in flow. This will delete the profile previously created
  // for the signed-in flow.
  static void CancelSignedInFlow();

  // Returns the path of the default profile used for rendering the picker.
  static base::FilePath GetPickerProfilePath();

  // Shows a dialog where the user can auth the profile or see the
  // auth error message. If a dialog is already shown, this destroys the current
  // dialog and creates a new one.
  static void ShowDialog(content::BrowserContext* browser_context,
                         const GURL& url,
                         const base::FilePath& profile_path);

  // Hides the dialog if it is showing.
  static void HideDialog();

  // Getter of the path of profile which is selected in profile picker for force
  // signin.
  static base::FilePath GetForceSigninProfilePath();

  // Getter of the target page  url. If not empty and is valid, it opens on
  // profile selection instead of the new tab page.
  static GURL GetOnSelectProfileTargetUrl();

  // Getter of the path of profile which is displayed on the profile switch
  // screen.
  static base::FilePath GetSwitchProfilePath();

  // Hides the profile picker.
  static void Hide();

  // Returns whether the profile picker is currently open.
  static bool IsOpen();

  // Returns whether the Profile picker is showing and active.
  static bool IsActive();

  // Returns the global profile picker view for testing.
  static views::View* GetViewForTesting();

  // Returns the web view (embedded in the picker) for testing.
  static views::WebView* GetWebViewForTesting();

  // Add a callback that will be called the next time the picker is opened.
  static void AddOnProfilePickerOpenedCallbackForTesting(
      base::OnceClosure callback);

  // Overrides the timeout delay for waiting for extended account info.
  static void SetExtendedAccountInfoTimeoutForTesting(base::TimeDelta timeout);

  // Returns a pref value indicating whether the profile picker has ever been
  // shown to the user.
  static bool Shown();

  // Returns whether to show profile picker at launch. This can be called on
  // startup or when Chrome is re-opened, e.g. when clicking on the dock icon on
  // MacOS when there are no windows, or from Windows tray icon.
  // This returns true if the user has multiple profiles and has not opted-out.
  static bool ShouldShowAtLaunch();
};

// Dialog that will be displayed when a locked profile is selected in the
// ProfilePicker when force-signin is enabled.
class ProfilePickerForceSigninDialog {
 public:
  // Dimensions of the reauth dialog displaying the password-separated signin
  // flow.
  static constexpr int kDialogHeight = 512;
  static constexpr int kDialogWidth = 448;

  // Shows a dialog where the user reauthenticates their primary account that
  // has invalid credentials, when force signin is enabled.
  static void ShowReauthDialog(content::BrowserContext* browser_context,
                               const std::string& email,
                               const base::FilePath& profile_path);

  // Shows a dialog where the user logs into their profile for the first time
  // via the profile picker, when force signin is enabled.
  static void ShowForceSigninDialog(content::BrowserContext* browser_context,
                                    const base::FilePath& profile_path);

  // Show the dialog and display local sign in error message without browser.
  static void ShowDialogAndDisplayErrorMessage(
      content::BrowserContext* browser_context);

  // Display local sign in error message without browser.
  static void DisplayErrorMessage();

  // Hides the dialog if it is showing.
  static void HideDialog();
};

#endif  // CHROME_BROWSER_UI_PROFILE_PICKER_H_
