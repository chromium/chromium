// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_client_ios.h"

namespace autofill {

// static
AutofillClientIOS* AutofillClientIOS::FromWebState(web::WebState* web_state) {
  return static_cast<AutofillClientIOS*>(web_state->GetUserData(UserDataKey()));
}

// static
const AutofillClientIOS* AutofillClientIOS::FromWebState(
    const web::WebState* web_state) {
  return static_cast<const AutofillClientIOS*>(
      web_state->GetUserData(UserDataKey()));
}

// Even though the destructor is marked as pure virtual (to prevent
// instantiation) it must be defined since it will be called by the
// sub-classes destructors.
AutofillClientIOS::~AutofillClientIOS() = default;

AutofillDriverIOSFactory& AutofillClientIOS::GetAutofillDriverFactory() {
  return autofill_driver_factory_;
}

AutofillClientIOS::AutofillClientIOS(web::WebState* web_state,
                                     id<AutofillDriverIOSBridge> bridge)
    : web_state_(web_state), autofill_driver_factory_(this, bridge) {}

// static
const void* AutofillClientIOS::UserDataKey() {
  static const int kId = 0;
  return &kId;
}

}  // namespace autofill
