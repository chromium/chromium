// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_POPUP_TYPES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_POPUP_TYPES_H_

namespace autofill {

// The list of all Autofill popup types that can be presented to the user.
enum class PopupType {
  kUnspecified,
  // Address form, but no address-related field is present. For example, it's
  // a sign-up page in which the user only enters the name and the email.
  kPersonalInformation,
  // Address form with address-related fields.
  kAddresses,
  kCreditCards,
  kPasswords,
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_POPUP_TYPES_H_
