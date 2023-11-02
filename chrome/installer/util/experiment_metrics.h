// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_EXPERIMENT_METRICS_H_
#define CHROME_INSTALLER_UTIL_EXPERIMENT_METRICS_H_

#include <stdint.h>

namespace installer {

// The various metrics reported for the status of the inactive user toast. This
// struct contains the information necessary to evaluate the efficacy of the
// toast on Chrome usage.
struct ExperimentMetrics {
 public:
  // The state of this install's participation in the experiment.
  enum State {
    kUninitialized = -1,

    // Relaunching setup.exe for a per-user install failed. Will retry on next
    // update.
    kRelaunchFailed = 0,

    // No user on console for per-machine install; waiting for invocation at
    // next logon via Active Setup.
    kWaitingForUserLogon = 1,

    // Timed out waiting for the setup singleton. Will retry on next update.
    kSingletonWaitTimeout = 2,

    // A group has been assigned. The experiment has moved out of the initial
    // state at this point. This state is reached under the setup singleton.
    kGroupAssigned = 3,

    // The user is not participating on account of using a tablet-like device.
    kIsTabletDevice = 4,

    // Chrome has been run within the last 28 days.
    kIsActive = 5,

    // The user has not been active on the machine much in the last 28 days.
    kIsDormant = 6,

    // Chrome has received an in-use update for which the rename is pending.
    // The UX cannot be shown.
    kIsUpdateRenamePending = 7,

    // Deferring presentation until it's okay to show the toast.
    kDeferringPresentation = 8,

    // Deferral was aborted on account of another process requiring the setup
    // singleton.
    kDeferredPresentationAborted = 9,

    // Launching Chrome for presentation.
    kLaunchingChrome = 10,

    // User selected 'No Thanks' button from UI after toast was shown.
    kSelectedNoThanks = 11,

    // User selected 'Open Chrome' button from UI after toast was shown but
    // Chrome crashed after opening.
    kSelectedOpenChromeAndCrash = 12,

    // User selected 'Open Chrome' button from UI after toast was shown and
    // user successfully opened chrome.
    kSelectedOpenChromeAndNoCrash = 13,

    // User selected [x] button from display.
    kSelectedClose = 14,

    // User logged off (gracefully) without interacting with toast.
    kUserLogOff = 15,

    // Another Chrome launch closed the toast.
    kOtherLaunch = 16,

    // The toast was closed via external means.
    kOtherClose = 17,

    NUM_STATES
  };

  // The location of the toast for those clients for which it was presented.
  enum ToastLocation {
    // The toast was shown positioned over Chrome's taskbar pin.
    kOverTaskbarPin = 0,

    // The toast was shown over the notification area.
    kOverNotificationArea = 1,
  };

  // Returns true if the install is in any of the states that precede group
  // assignment.
  bool InInitialState() const;

  // Returns true if the install is in a terminal state and should no longer
  // participate in the experiment.
  bool InTerminalState() const;

  bool operator==(const ExperimentMetrics& other) const;

  // The number of experiment groups (including the  holdback group).
  enum : int { kNumGroups = 17 };

  // The index of the holdback group.
  enum : int { kHoldbackGroup = kNumGroups - 1 };

  // Unix epoch of time from when time bucket for experiment is started.
  // This will be subtracted from the day the toast was shown to bucket user
  // into cohorts for analysing retention. (31 Aug 2017 00:00:00 PST)
  enum : int64_t { kExperimentStartSeconds = 1504162800 };

  // Maximum number of time toast should be displayed (3 bits).
  enum : int { kMaxToastCount = 7 };

  // Maximum value of first toast offset. (10 bits).
  enum : int { kMaxFirstToastOffsetDays = 1023 };

  // Maximum value of last used bucket. (7 bits, in days and log scale).
  enum : int { kMaxLastUsed = 1825 };  // 5 yr in days.

  // Maximum value of user session length. (6 bits, in minutes and log scale).
  enum : int { kMaxSessionLength = 40320 };  // 28 days in minutes.

  // Maximum value of user session length. (5 bits, in seconds and log scale).
  enum : int { kMaxActionDelay = 604800 };  // 7 days in seconds.

  enum : int { kSessionLengthBucketBits = 6 };

  enum : int { kActionDelayBucketBits = 5 };

  enum : int { kLastUsedBucketBits = 7 };

  enum : int { kToastHourBits = 5 };

  enum : int { kFirstToastOffsetBits = 10 };

  enum : int { kToastCountBits = 3 };

  enum : int { kToastLocationBits = 1 };

  enum : int { kStateBits = 5 };

  enum : int { kGroupBits = 5 };

  // The group to which this install has been assigned.
  int group = 0;

  State state = kUninitialized;
  ToastLocation toast_location = kOverTaskbarPin;

  // The number of times the toast has been presented.
  int toast_count = 0;

  // The number of days that have passed since (13 Jun 2017 00:00:00 PST) on
  // the first day the toast was presented.
  int first_toast_offset_days = 0;

  // The local (wall clock) hour in which the toast was presented.
  int toast_hour = 0;

  // Days since the last time Chrome was used in the range [1-1825) in a
  // 128-bucket log scale.
  int last_used_bucket = 0;

  // Time delta (in seconds) between presentation and action in the range
  // [1-604800) in a 32-bucket log scale.
  int action_delay_bucket = 0;

  // Time delta (in minutes) between user session start and presentation in the
  // range [1-40320) in a 64-bucket log scale.
  int session_length_bucket = 0;
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_EXPERIMENT_METRICS_H_
