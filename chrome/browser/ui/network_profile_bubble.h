// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_NETWORK_PROFILE_BUBBLE_H_
#define CHROME_BROWSER_UI_NETWORK_PROFILE_BUBBLE_H_

class Browser;
class Profile;

namespace base {
class FilePath;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// This class will try to detect if the profile is on a network share and if
// this is the case notify the user with an info bubble.
class NetworkProfileBubble {
 public:
  enum MetricNetworkedProfileCheck {
    // Check was suppressed by command line flag.
    METRIC_CHECK_SUPPRESSED,
    // WTSQuerySessionInformation call failed.
    METRIC_CHECK_FAILED,
    // File access in profile dir failed.
    METRIC_CHECK_IO_FAILED,

    // Profile on a network share detected.
    METRIC_PROFILE_ON_NETWORK,
    // Profile not on a network share detected.
    METRIC_PROFILE_NOT_ON_NETWORK,

    // Check was suppressed because of remote session.
    METRIC_REMOTE_SESSION,

    // User has clicked learn more on the notification bubble.
    METRIC_LEARN_MORE_CLICKED,
    // User has clicked OK on the notification bubble.
    METRIC_ACKNOWLEDGED,

    METRIC_NETWORKED_PROFILE_CHECK_SIZE  // Must be the last.
  };

  NetworkProfileBubble() = delete;
  NetworkProfileBubble(const NetworkProfileBubble&) = delete;
  NetworkProfileBubble& operator=(const NetworkProfileBubble&) = delete;

  // Returns true if the check for network located profile should be done. This
  // test is only performed up to |kMaxWarnings| times in a row and then
  // repeated after a period of silence that lasts |kSilenceDurationDays| days.
  static bool ShouldCheckNetworkProfile(Profile* profile);

  // Verifies that the profile folder is not located on a network share, and if
  // it is shows the warning bubble to the user.
  static void CheckNetworkProfile(const base::FilePath& profile_folder);

  // Shows the notification bubble using the provided |browser|.
  static void ShowNotification(Browser* browser);

  static void SetNotificationShown(bool shown);

  // Register the pref that controls whether the bubble should be shown anymore.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Helper function wrapping the UMA_HISTOGRAM_ENUMERATION macro.
  static void RecordUmaEvent(MetricNetworkedProfileCheck event);

 private:
  // This function creates the notification bubble, attaches it to the
  // |anchor| View and then shows it to the user.
  static void NotifyNetworkProfileDetected();

  // Set to true once the notification check has been performed to avoid showing
  // the notification more than once per browser run.
  // This flag is not thread-safe and should only be accessed on the UI thread!
  static bool notification_shown_;
};

#endif  // CHROME_BROWSER_UI_NETWORK_PROFILE_BUBBLE_H_
