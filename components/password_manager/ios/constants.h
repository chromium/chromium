// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_CONSTANTS_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_CONSTANTS_H_

enum class AcceptedGeneratedPasswordSourceType {
  // The generated password was accepted from the proactive bottom sheet.
  kProactiveBottomSheet,

  // The generated password was accepted from the "Suggest strong password"
  // keyboard accessory.
  kSuggestion,

  // The generated password was accepted from the manual fallback keyboard
  // accessory.
  kManualFallback,

  // Number of enum entries, used for UMA histogram reporting macros.
  kMaxValue = kManualFallback
};

enum class PasswordGenerationBottomSheetStateTransitionType {
  // The proactive password generation bottom sheet has been silenced due to
  // being dismissed too many times. It won't be re-triggered until unsilenced.
  kSilenced,

  // The proactive password generation bottom sheet has been unsilenced due to a
  // generated password being accepted from the keyboard accessory. It can now
  // be re-triggered until silenced again.
  kUnsilenced,

  // Number of enum entries, used for UMA histogram reporting macros.
  kMaxValue = kUnsilenced
};

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_CONSTANTS_H_
