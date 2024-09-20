// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROFILES_PROFILE_PICKER_H_
#define CHROME_BROWSER_UI_PROFILES_PROFILE_PICKER_H_

#include <optional>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

class GURL;
class Profile;

namespace views {
class View;
class WebView;
}  // namespace views

enum class StartupProfileModeReason;
class ForceSigninUIError;

class ProfilePicker {
 public:
  // This is used for logging, so do not remove or reorder existing entries.
  enum class FirstRunExitStatus {
    // The user completed the FRE and is continuing to launch the browser.
    kCompleted = 0,

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // The user exited the FRE before going through the mandatory steps.
    kQuitEarly = 1,
#endif

    // The user finished the mandatory FRE steps but abandoned their task
    // (closed the browser app).
    kQuitAtEnd = 2,

    // The user opens the first run again while it's still running.
    kAbortTask = 3,

    // The user does something that bypasses the FRE (opens new window etc..).
    kAbandonedFlow = 4,

    // Add any new values above this one, and update kMaxValue to the highest
    // enumerator value.
    kMaxValue = kAbandonedFlow
  };
  using FirstRunExitedCallback =
      base::OnceCallback<void(FirstRunExitStatus status)>;

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
    // A new session was started while Chrome was already running (e.g. by
    // clicking on the tray icon on Windows).
    kNewSessionOnExistingProcess = 4,
    kProfileLocked = 5,
    kUnableToCreateBrowser = 6,
    kBackgroundModeManager = 7,
    // May only be used on lacros, opens an account picker, listing all accounts
    // that are not used in the provided profile, yet.
    kLacrosSelectAvailableAccount = 8,
    // May only be used on lacros, opens a first run experience (provided no
    // policies prevent it) to let the user opt in to sync, etc. for the primary
    // profile.
    // TODO(crbug.com/40242849): Migrate to only using kFirstRun.
    kLacrosPrimaryProfileFirstRun = 9,
    // The Profile became idle, due to the IdleProfileCloseTimeout policy.
    kProfileIdle = 10,
    // Opens the first run experience on non-Lacros desktop platforms to let the
    // user sign in, opt in to sync, etc.
    kFirstRun = 11,
    // There was no usable profile on startup (e.g. the profiles were locked by
    // force signin).
    kOnStartupNoProfile = 12,
    // There was no usable profile when starting a new session while Chrome is
    // already running (e.g. the profiles were locked by force signin).
    kNewSessionOnExistingProcessNoProfile = 13,
    // Opens manage profile view from the app menu.
    kAppMenuProfileSubMenuManageProfiles = 14,
    // Opens the add new profile view from the app menu.
    kAppMenuProfileSubMenuAddNewProfile = 15,

    kMaxValue = kAppMenuProfileSubMenuAddNewProfile,
  };

  class Params final {
   public:
    // Basic constructors and operators.
    ~Params();
    Params(Params&&);
    Params& operator=(Params&&);

    Params(const Params&) = delete;
    Params& operator=(const Params&) = delete;

    // Basic constructor. Specifies only the entry point, and all other
    // parameters have default values. Use specialized entry points when they
    // are available (e.g. `ForBackgroundManager()`).
    static Params FromEntryPoint(EntryPoint entry_point);

    // Builds parameter with the `kBackgroundModeManager` entry point. Allows
    // specifying extra parameters.
    static Params ForBackgroundManager(
        const GURL& on_select_profile_target_url);

    EntryPoint entry_point() const { return entry_point_; }

    // Returns the path to the profile to use to display the Web UI.
    const base::FilePath& profile_path() const { return profile_path_; }

    // May be non-empty only for the `kBackgroundModeManager` entry point.
    const GURL& on_select_profile_target_url() const {
      return on_select_profile_target_url_;
    }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Builds parameter with the `kLacrosSelectAvailableAccount` entry point.
    //
    // `profile_path` specifies the profile that should be used to render
    // the profile picker. If `profile_path` matches the current value
    // for an existing picker, then `Show()` reactivates the existing picker.
    // Otherwise `Show()` hides the current window and shows a new one.
    //
    // `account_selected_callback` is called when the user picks an account on
    // the account selection screen. If the user closes the window, it is called
    // with the empty string. If the user clicks "Use another account" and
    // starts an OS account addition, this callback is passed to
    // `ShowAddAccountDialog()` and will be called with its result.
    static Params ForLacrosSelectAvailableAccount(
        const base::FilePath& profile_path,
        base::OnceCallback<void(const std::string&)> account_selected_callback);

    // Calls `account_selected_callback_`. See
    // `ForLacrosSelectAvailableAccount()` for more details.
    void NotifyAccountSelected(const std::string& gaia_id);
#endif

    // Builds parameter with the `kFirstRun` (on Dice) or the
    //  `kLacrosPrimaryProfileFirstRun` (on Lacros) entry point.
    //
    // `profile_path` is the profile for which to open the FRE. On Lacros we
    // expect it to be the main profile path.
    // `first_run_exited_callback` is called when the first run experience is
    // exited, with a `FirstRunExitStatus` indicating how the user responded to
    // it.
    static Params ForFirstRun(const base::FilePath& profile_path,
                              FirstRunExitedCallback first_run_exited_callback);

    // Calls `first_run_exited_callback_`, forwarding `exit_status`.See
    // `ForFirstRun()` for more details.
    //
    // If this method is not called by the time this `Param` is destroyed, an
    // intent to quit will be assumed and `first_run_exited_callback_` will be
    // called by the destructor with quit-related arguments.
    void NotifyFirstRunExited(FirstRunExitStatus exit_status);

    // Returns whether the current profile picker window can be reused for
    // different parameters. If this returns false, the picker cannot be reused
    // and must be closed and repoen.
    bool CanReusePickerWindow(const Params& other) const;

   private:
    // Constructor is private, use static functions instead.
    explicit Params(EntryPoint entry_point, const base::FilePath& profile_path);

    EntryPoint entry_point_ = EntryPoint::kOnStartup;
    GURL on_select_profile_target_url_;
    base::FilePath profile_path_;
    FirstRunExitedCallback first_run_exited_callback_;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    base::OnceCallback<void(const std::string&)> account_selected_callback_;

#endif
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

  // Shows the Profile picker for the given `Params` or re-activates an existing
  // one (see `Params::ForAccountSelecAvailableAccount()` for details on
  // re-activation). When reactivated, the displayed page is not updated.
  static void Show(Params&& params);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Helper struct to allow passing different profile information for sign in:
  // - An optional color for a new profile.
  // - A file path for an existing profile.
  using ProfileInfo = absl::variant<std::optional<SkColor>, base::FilePath>;

  // Starts the Dice sign-in flow. The layout of the window gets updated for the
  // sign-in flow while the profiles are created/loaded.
  // The sign in flow can be triggered for a new or existing profile.
  // For new profiles, the expected color is expected to be given as the
  // `profile_info` param. The creation of the new profile happens and then the
  // sign-in page is rendered using the new profile. The new profile uses a
  // theme generated from the given profile color if provided or the default
  // theme.
  // For an existing profile, the profile path is expected to given as the
  // `profile_info` param. The profile is loaded then the sign-page will be
  // rendered with the profile.
  // `switch_finished_callback` gets informed whether the creation of the new
  // profile succeeded and the sign-in page gets displayed.
  static void SwitchToDiceSignIn(
      ProfileInfo profile_info,
      base::OnceCallback<void(bool)> switch_finished_callback);

  // Starts the reauth for the existing primary account in the given `profile`.
  // The flow will remain within the profile picker. The reauth is expected to
  // be done only on the primary account, if done on another one (the UI may
  // allow it), the reauth will fail and the signed in account will be signed
  // out.
  // On successful reauth, the profile is unlocked and a browser associated with
  // the `profile` will be opened. On unsuccessful reauth, the user will be
  // redirected to the profile picker main page, with a popup error dialog
  // displayed through `on_error_callback`.
  static void SwitchToReauth(
      Profile* profile,
      base::OnceCallback<void(const ForceSigninUIError&)> on_error_callback);
#endif

  // Switch to the flow that comes when the user decides to create a profile
  // without signing in.
  // `profile_color` is the profile's color. It is undefined for the default
  // theme.
  // `profile_picked_time_on_startup` is the time when the user picked a
  // profile to open, to measure browser startup performance. It is only set
  // when the picker is shown on startup.
  static void SwitchToSignedOutPostIdentityFlow(
      std::optional<SkColor> profile_color,
      base::TimeTicks profile_picked_time_on_startup,
      base::OnceCallback<void(bool)> switch_finished_callback);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Starts the flow to set-up a signed-in profile. `signed_in_profile` must
  // have an unconsented primary account.
  static void SwitchToSignedInFlow(std::optional<SkColor> profile_color,
                                   Profile* signed_in_profile);
#endif

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
  static void ShowDialog(Profile* profile, const GURL& url);

  // Hides the dialog if it is showing.
  static void HideDialog();

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

  // Returns whether the profile picker is currently open and showing the First
  // Run Experience.
  static bool IsFirstRunOpen();

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
  static StartupProfileModeReason GetStartupModeReason();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Calls the callback passed to
  // `ProfilePicker::Params::ForLacrosSelectAvailableAccount()`.
  static void NotifyAccountSelected(const std::string& gaia_id);
#endif

  // Show the dialog and display local sign in error message without browser.
  static void ShowDialogAndDisplayErrorMessage(Profile* profile);
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
  static void ShowReauthDialog(Profile* profile, const std::string& email);

  // Shows a dialog where the user logs into their profile for the first time
  // via the profile picker, when force signin is enabled.
  static void ShowForceSigninDialog(Profile* profile);

  // Display local sign in error message without browser.
  static void DisplayErrorMessage();
};

#endif  // CHROME_BROWSER_UI_PROFILES_PROFILE_PICKER_H_
