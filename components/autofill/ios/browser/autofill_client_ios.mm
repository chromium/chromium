// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_client_ios.h"

#import "base/containers/flat_set.h"
#import "base/no_destructor.h"

namespace autofill {

namespace {

base::flat_set<AutofillClientIOS::FromWebStateImpl>& GetFromWebStateImpls() {
  static base::NoDestructor<base::flat_set<AutofillClientIOS::FromWebStateImpl>>
      g_from_web_state_impls;
  return *g_from_web_state_impls;
}

}  // namespace

// static
AutofillClientIOS* AutofillClientIOS::FromWebState(web::WebState* web_state) {
  for (FromWebStateImpl from_web_state_impl : GetFromWebStateImpls()) {
    if (AutofillClientIOS* client = from_web_state_impl(web_state)) {
      return client;
    }
  }
  return nullptr;
}

AutofillClientIOS::AutofillClientIOS(FromWebStateImpl from_web_state_impl,
                                     web::WebState* web_state,
                                     id<AutofillDriverIOSBridge> bridge)
    : web_state_(web_state->GetWeakPtr()),
      autofill_driver_factory_(this, bridge) {
  CHECK(from_web_state_impl);
  GetFromWebStateImpls().insert(from_web_state_impl);
}

AutofillClientIOS::~AutofillClientIOS() = default;

AutofillDriverIOSFactory& AutofillClientIOS::GetAutofillDriverFactory() {
  return autofill_driver_factory_;
}

}  // namespace autofill
