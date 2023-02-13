// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_FACTORY_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_FACTORY_H_

#include <string>

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/ios/browser/autofill_driver_ios_bridge.h"
#include "ios/web/public/web_state_user_data.h"

namespace web {
class WebFrame;
class WebState;
}  // namespace web

namespace autofill {

class AutofillDriverIOS;

// This factory will keep the parameters needed to create an AutofillDriverIOS.
// These parameters only depend on the web_state, so there is one
// AutofillDriverIOSFactory per WebState.
class AutofillDriverIOSFactory
    : public web::WebStateUserData<AutofillDriverIOSFactory> {
 public:
  ~AutofillDriverIOSFactory() override;

  // Returns the AutofillDriverIOS for `web_frame`. Creates the driver if
  // necessary.
  AutofillDriverIOS* DriverForFrame(web::WebFrame* web_frame);

 private:
  friend class web::WebStateUserData<AutofillDriverIOSFactory>;

  // Creates a AutofillDriverIOSFactory that will store all the
  // needed to create a AutofillDriverIOS.
  AutofillDriverIOSFactory(web::WebState* web_state,
                           AutofillClient* client,
                           id<AutofillDriverIOSBridge> bridge,
                           const std::string& app_locale);

  web::WebState* web_state_ = nullptr;
  AutofillClient* client_ = nullptr;
  id<AutofillDriverIOSBridge> bridge_ = nil;
  std::string app_locale_;
  WEB_STATE_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_FACTORY_H_
