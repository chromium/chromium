// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_PLUS_ADDRESS_SURVEY_TYPE_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_PLUS_ADDRESS_SURVEY_TYPE_H_

// TODO: crbug.com/348139343 - Move back to components/plus_addresses.
namespace plus_addresses::hats {

// Specifies the type of feature perception flow to launch for the user.
enum class SurveyType {
  // Triggered after the user has created their first plus address.
  kAcceptedFirstTimeCreate = 1,
  // The user has declined the first plus address creation flow.
  kDeclinedFirstTimeCreate = 2,
  // The user has created their 3rd, 4th, ... plus address.
  kCreatedMultiplePlusAddresses = 3,
  // The user has created a plus address by triggering the popup via the Chrome
  // context menu.
  kCreatedPlusAddressViaManualFallback = 4,
  // The user was shown a plus address suggestion and potentially some number of
  // other suggestions. The user accepted the plus address suggestion.
  kDidChoosePlusAddressOverEmail = 5,
  // The user was shown both an email and a plus address suggestion. The user
  // accepted the email suggestion.
  kDidChooseEmailOverPlusAddress = 6,
  // The user triggered plus address manual fallback from the Chrome context
  // menu on Desktop or from the keyboard accessory and filled a plus address
  // into a web input field.
  kFilledPlusAddressViaManualFallack = 7,
};

}  // namespace plus_addresses::hats

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_PLUS_ADDRESS_SURVEY_TYPE_H_
