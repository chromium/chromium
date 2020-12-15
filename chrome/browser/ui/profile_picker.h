// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROFILE_PICKER_H_
#define CHROME_BROWSER_UI_PROFILE_PICKER_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace content {
class BrowserContext;
}

namespace views {
class View;
class WebView;
}  // namespace views

class ProfilePicker {
 public:
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
    kMaxValue = kNewSessionOnExistingProcess,
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

  // Shows the Profile picker for the given `entry_point` or re-activates an
  // existing one. In the latter case, the displayed page is not updated.
  static void Show(EntryPoint entry_point);

  // Starts the sign-in flow. The layout of the window gets updated for the
  // sign-in flow. At the same time, the new profile is created (with
  // `profile_color`) and the sign-in page is rendered using the new profile.
  // `switch_finished_callback` gets informed whether the creation of the new
  // profile succeeded and the sign-in page gets displayed.
  static void SwitchToSignIn(
      SkColor profile_color,
      base::OnceCallback<void(bool)> switch_finished_callback);

  // Finishes the sign-in flow by moving to the sync confirmation screen. It
  // uses the same new profile created by `SwitchToSignIn()`.
  static void SwitchToSyncConfirmation();

  // Shows a dialog where the user can auth the profile or see the
  // auth error message. If a dialog is already shown, this destroys the current
  // dialog and creates a new one.
  static void ShowDialog(content::BrowserContext* browser_context,
                         const GURL& url,
                         const base::FilePath& profile_path);

  // Hides the dialog if it is showing.
  static void HideDialog();

  // Displays sign in error message that is created by Chrome but not GAIA
  // without browser window. If the dialog is not currently shown, this does
  // nothing.
  static void DisplayErrorMessage();

  // Getter of the path of profile which is selected in profile picker for force
  // signin.
  static base::FilePath GetForceSigninProfilePath();

  // Hides the profile picker.
  static void Hide();

  // Returns whether the profile picker is currently open.
  static bool IsOpen();

  // Returns the global profile picker view for testing.
  static views::View* GetViewForTesting();

  // Returns the web view (embedded in the picker) for testing.
  static views::WebView* GetWebViewForTesting();

  // Returns the simple toolbar (embedded in the picker) for testing.
  static views::View* GetToolbarForTesting();

  // Overrides the timeout delay for waiting for extended account info.
  static void SetExtendedAccountInfoTimeoutForTesting(base::TimeDelta timeout);

  // Returns whether to show profile picker at launch. This can be called on
  // startup or when Chrome is re-opened, e.g. when clicking on the dock icon on
  // MacOS when there are no windows, or from Windows tray icon.
  // This returns true if the user has multiple profiles and has not opted-out.
  static bool ShouldShowAtLaunch();

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfilePicker);
};

#endif  // CHROME_BROWSER_UI_PROFILE_PICKER_H_
