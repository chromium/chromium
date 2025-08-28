// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_CLIENT_IOS_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_CLIENT_IOS_H_

#import "base/memory/raw_ptr.h"
#import "base/supports_user_data.h"
#import "components/autofill/core/browser/foundations/autofill_client.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "ios/web/public/web_state.h"

namespace autofill {

// Common base class for all AutofillClients on iOS.
//
// There is at most one instance per web::WebState, and the instance are
// owned by the web::WebState to which they are attached.
class AutofillClientIOS : public AutofillClient,
                          public base::SupportsUserData::Data {
 public:
  // Returns the instance that is associated with `web_state`, if any exists.
  static AutofillClientIOS* FromWebState(web::WebState* web_state);
  static const AutofillClientIOS* FromWebState(const web::WebState* web_state);

  // The destructor is marked as pure-virtual to prevent instantiation of
  // this class. Only sub-classes can be instantiated.
  ~AutofillClientIOS() override = 0;

  // Non-null throughout the lifetime of the AutofillClientIOS.
  web::WebState* web_state() const { return web_state_.get(); }

  // Intentionally final to allow it to be called during construction (in
  // particular, transitively by members of subclasses).
  AutofillDriverIOSFactory& GetAutofillDriverFactory() final;

 private:
  template <typename Derived, typename Super>
  friend class AutofillClientIOSMixin;

  // Default constructor.
  AutofillClientIOS(web::WebState* web_state,
                    id<AutofillDriverIOSBridge> bridge);

  // Returns the key used to attach an AutofillClientIOS to a web::WebState.
  static const void* UserDataKey();

  // The owning WebState.
  const raw_ptr<web::WebState> web_state_;
  AutofillDriverIOSFactory autofill_driver_factory_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_CLIENT_IOS_H_
