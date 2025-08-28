// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_CLIENT_IOS_MIXIN_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_CLIENT_IOS_MIXIN_H_

#import "base/check.h"
#import "base/memory/ptr_util.h"
#import "components/autofill/ios/browser/autofill_client_ios.h"
#import "ios/web/public/web_state.h"

namespace autofill {

// Helper class allowing to simplify writing sub-classes of AutofillClientIOS
// by taking care of generating the boilerplate methods.
template <typename Derived, typename Super = AutofillClientIOS>
class AutofillClientIOSMixin : public Super {
 public:
  // Creates an object of type T, and attaches it to the specified WebState.
  // If an instance is already attached, does nothing.
  template <typename... Args>
  static void CreateForWebState(web::WebState* web_state, Args&&... args) {
    CHECK(web_state);
    CHECK(!web_state->IsBeingDestroyed());
    if (!FromWebState(web_state)) {
      web_state->SetUserData(AutofillClientIOS::UserDataKey(),
                             base::WrapUnique(new Derived(
                                 web_state, std::forward<Args>(args)...)));
    }
  }

  // Returns the instance that is associated with `web_state`, if any exists.
  static Derived* FromWebState(web::WebState* web_state) {
    return static_cast<Derived*>(Super::FromWebState(web_state));
  }

  static const Derived* FromWebState(const web::WebState* web_state) {
    return static_cast<const Derived*>(Super::FromWebState(web_state));
  }

  ~AutofillClientIOSMixin() override = default;

 protected:
  // Default constructor.
  template <typename... Args>
  AutofillClientIOSMixin(web::WebState* web_state, Args&&... args)
      : Super(web_state, std::forward<Args>(args)...) {}
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_CLIENT_IOS_MIXIN_H_
