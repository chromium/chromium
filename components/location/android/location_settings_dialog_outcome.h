// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LOCATION_ANDROID_LOCATION_SETTINGS_DIALOG_OUTCOME_H_
#define COMPONENTS_LOCATION_ANDROID_LOCATION_SETTINGS_DIALOG_OUTCOME_H_

// An enum to describe the outcome of a system location setting prompt.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.location
enum LocationSettingsDialogOutcome {
  // The user accepted the prompt and the system location setting has been
  // flipped to granted.
  GRANTED = 1,

  // The user rejected the prompt and the system location setting has not been
  // flipped.
  DENIED = 2,

  // The prompt could not be triggered.
  // When CanPromptToEnableSystemLocationSetting() returns true, this should
  // only happen in exceptional circonstances, e.g.
  // - In the case of a race condition where the system location setting is
  // flipped elsewhere before the prompt could be triggered;
  // - In the case where some of the features required to trigger a system
  // location setting prompt became unavailable or unresponsive after the
  // response from CanPromptToEnableSystemLocationSetting() was received.
  NO_PROMPT = 3,
};

#endif  // COMPONENTS_LOCATION_ANDROID_LOCATION_SETTINGS_DIALOG_OUTCOME_H_
