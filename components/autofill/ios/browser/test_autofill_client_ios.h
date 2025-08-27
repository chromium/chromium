// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_TEST_AUTOFILL_CLIENT_IOS_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_TEST_AUTOFILL_CLIENT_IOS_H_

#import "components/autofill/core/browser/foundations/test_autofill_client.h"
#import "components/autofill/ios/browser/autofill_client_ios.h"

namespace web {
class WebState;
}

namespace autofill {

// Mixin that registers `T` with a test-only registry so that
// AutofillClientIOS::FromWebState() finds `T`.
template <typename T>
  requires(std::derived_from<T, AutofillClientIOS>)
class WithFakedFromWebState : public T {
 public:
  template <typename... Args>
  explicit WithFakedFromWebState(Args&&... args)
      : T(std::forward<Args>(args)...) {}

  ~WithFakedFromWebState() override {
    if (T::web_state()) {
      // The AutofillClientIOS contract requires a call of
      // AutofillDriverIOSFactory::WebStateDestroyed() before any members of the
      // client are destroyed.
      static_cast<web::WebStateObserver&>(T::GetAutofillDriverFactory())
          .WebStateDestroyed(T::web_state());
    }
  }
};

// A variant of TestAutofillClient that is be associated with a web::WebState.
using TestAutofillClientIOS =
    WithFakedFromWebState<TestAutofillClientTemplate<AutofillClientIOS>>;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_TEST_AUTOFILL_CLIENT_IOS_H_
