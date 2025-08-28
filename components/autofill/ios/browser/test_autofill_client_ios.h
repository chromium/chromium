// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_TEST_AUTOFILL_CLIENT_IOS_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_TEST_AUTOFILL_CLIENT_IOS_H_

#import "components/autofill/core/browser/foundations/test_autofill_client.h"
#import "components/autofill/ios/browser/autofill_client_ios.h"
#import "components/autofill/ios/browser/autofill_client_ios_mixin.h"

namespace web {
class WebState;
}

namespace autofill {

// A variant of TestAutofillClient that is be associated with a web::WebState.
class TestAutofillClientIOS
    : public AutofillClientIOSMixin<
          TestAutofillClientIOS,
          TestAutofillClientTemplate<AutofillClientIOS>> {
 private:
  friend class AutofillClientIOSMixin<
      TestAutofillClientIOS,
      TestAutofillClientTemplate<AutofillClientIOS>>;

  TestAutofillClientIOS(web::WebState* web_state,
                        id<AutofillDriverIOSBridge> bridge)
      : AutofillClientIOSMixin<TestAutofillClientIOS,
                               TestAutofillClientTemplate<AutofillClientIOS>>(
            web_state,
            bridge) {}
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_TEST_AUTOFILL_CLIENT_IOS_H_
