// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ACCESSORY_SHEET_ENUMS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ACCESSORY_SHEET_ENUMS_H_

namespace autofill {

// Describes the different types of accessory sheets.
// Used to record metrics specific to a tab types (e.g. passwords, payments).
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Must be kept in sync with the suffix
// AccessorySheetType in histogram.xml. A java IntDef@ is generated from this.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.keyboard_accessory
enum class AccessoryTabType {
  ALL = 0,
  PASSWORDS = 1,
  CREDIT_CARDS = 2,
  ADDRESSES = 3,
  TOUCH_TO_FILL = 4,
  COUNT,
};

// Describes possible actions in the keyboard accessory and its sheets. Used to
// distinguish specific actions and links.
// Additionally, they are used to record metrics for the associated action.
// Therefore, entries should not be renumbered and numeric values should never
// be reused. Must be kept in sync with the enum in enums.xml. A java IntDef@ is
// generated from this.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.keyboard_accessory
enum class AccessoryAction {
  GENERATE_PASSWORD_AUTOMATIC = 0,
  MANAGE_PASSWORDS = 1,
  AUTOFILL_SUGGESTION = 2,
  MANAGE_CREDIT_CARDS = 3,
  MANAGE_ADDRESSES = 4,
  GENERATE_PASSWORD_MANUAL = 5,
  COUNT,
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ACCESSORY_SHEET_ENUMS_H_
