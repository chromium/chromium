// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_CLIENT_IOS_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_CLIENT_IOS_H_

#import "base/memory/raw_ptr.h"
#import "components/autofill/core/browser/foundations/autofill_client.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "ios/web/public/web_state.h"

namespace autofill {

// Common base class for all AutofillClients on iOS.
//
// There must be at most one instance per web::WebState. Generally,
// AutofillClientIOS should be owned (perhaps indirectly) by a
// web::WebStateUserData object.
class AutofillClientIOS : public AutofillClient {
 public:
  // A function pointer rather than a base::RepeatingCallback to facilitate
  // insertion into a set.
  using FromWebStateImpl = AutofillClientIOS* (*)(web::WebState*);

  // Returns the `AutofillClientIOS` that is associated with `web_state`, if any
  // exists.
  static AutofillClientIOS* FromWebState(web::WebState* web_state);

  // At most one `AutofillClientIOS` may exist per `web_state` at any point in
  // time.
  //
  // Callers must guarantee that after construction of an
  // `AutofillClientIOS(from_web_state_impl, web_state, bridge)`,
  // `from_web_state_impl(web_state)` returns a pointer to this
  // AutofillClientIOS.
  // If and when that is the case, `FromWebState(web_state)` returns the
  // AutofillClientIOS.
  //
  // Typically, `from_web_state_impl` is identical for all instances of a
  // specific subclass of AutofillClientIOS.
  //
  // For example, ChromeAutofillClientIOS is owned by AutofillTabHelper, which
  // is WebStateUserData. Therefore, all ChromeAutofillClientIOS instances pass
  // a pointer to the function calls AutofillTabHelper::FromWebContents() and
  // then returns the AutofillTabHelper's AutofillClientIOS.
  // Similarly for WebAutofillClientIOS and other implementations.
  AutofillClientIOS(FromWebStateImpl from_web_state_impl,
                    web::WebState* web_state,
                    id<AutofillDriverIOSBridge> bridge);
  AutofillClientIOS(const AutofillClientIOS&) = delete;
  AutofillClientIOS& operator=(const AutofillClientIOS&) = delete;
  ~AutofillClientIOS() override;

  web::WebState* web_state() const { return web_state_.get(); }

  // Intentionally final to allow it to be called during construction (in
  // particular, transitively by members of subclasses).
  AutofillDriverIOSFactory& GetAutofillDriverFactory() final;

 private:
  // Currently, this dangles at the time of ~AutofillClientIOS() because
  // ~WebState() has already run at the time.
  // TODO(crbug.com/380442588): Change to WeakPtr and define the contract. Also
  // explain how and why the destruction order works in production code in
  // Chrome and WebView.
  raw_ptr<web::WebState> web_state_;

  AutofillDriverIOSFactory autofill_driver_factory_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_CLIENT_IOS_H_
