// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_SMART_LOCK_METRICS_RECORDER_H_
#define CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_SMART_LOCK_METRICS_RECORDER_H_

class SmartLockMetricsRecorder {
 public:
  SmartLockMetricsRecorder();
  ~SmartLockMetricsRecorder();

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/enums.xml, and should always reflect it (do not
  // change one without changing the other). Entries should be never modified
  // or deleted. Only additions possible.
  enum class SmartLockAuthResultFailureReason {
    kUnlockNotAllowed = 0,
    // kDeprecatedAlreadyAttemptingAuth = 1, (obsolete)
    kEmptyUserAccount = 2,
    kInvalidAccoundId = 3,
    kAuthAttemptCannotStart = 4,
    kNoPendingOrActiveHost = 5,
    kAuthenticatedChannelDropped = 6,
    kFailedToSendUnlockRequest = 7,
    // kFailedToDecryptSignInChallenge = 8, (obsolete)
    kFailedtoNotifyHostDeviceThatSmartLockWasUsed = 9,
    kAuthAttemptTimedOut = 10,
    kUnlockEventSentButNotAttemptingAuth = 11,
    kUnlockRequestSentButNotAttemptingAuth = 12,
    // kLoginDisplayHostDoesNotExist = 13, (obsolete)
    // kUserControllerSignInFailure = 14, (obsolete)
    kMaxValue = kUnlockRequestSentButNotAttemptingAuth
  };

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/enums.xml, and should always reflect it (do not
  // change one without changing the other). Entries should be never modified
  // or deleted. Only additions possible.
  enum class SmartLockAuthMethodChoice {
    kSmartLock = 0,
    kOther = 1,
    kMaxValue = kOther
  };

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/enums.xml, and should always reflect it (do not
  // change one without changing the other). Entries should be never modified
  // or deleted. Only additions possible.
  enum class SmartLockAuthEventPasswordState {
    kUnknownState = 0,
    // kNoPairing = 1, (obsolete)
    // kPairingChanged = 2, (obsolete)
    // kUserHardlock = 3, (obsolete)
    kServiceNotActive = 4,
    kNoBluetooth = 5,
    kBluetoothConnecting = 6,
    kCouldNotConnectToPhone = 7,
    kNotAuthenticated = 8,
    kPhoneLocked = 9,
    kRssiTooLow = 10,
    kAuthenticatedPhone = 11,
    // kLoginFailed = 12, (obsolete)
    // kPairingAdded = 13, (obsolete)
    // kNoScreenlockStateHandler = 14, (obsolete)
    kPhoneLockedAndRssiTooLow = 15,
    // kForcedReauth = 16, (obsolete)
    // kLoginWithSmartLockDisabled = 17, (obsolete)
    kPhoneNotLockable = 18,
    kPrimaryUserAbsent = 19,
    kMaxValue = kPrimaryUserAbsent
  };

  static void RecordSmartLockUnlockAuthMethodChoice(
      SmartLockAuthMethodChoice auth_method_choice);

  static void RecordAuthResultUnlockSuccess(bool success = true);
  static void RecordAuthResultUnlockFailure(
      SmartLockAuthResultFailureReason failure_reason);

  static void RecordAuthMethodChoiceUnlockPasswordState(
      SmartLockAuthEventPasswordState password_state);

 private:
  static void RecordAuthResultSuccess(bool success);
};

#endif  // CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_SMART_LOCK_METRICS_RECORDER_H_
