// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_FACTORY_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_FACTORY_H_

#import <string>

#import "base/memory/raw_ptr.h"
#import "base/types/pass_key.h"
#import "components/autofill/core/browser/autofill_client.h"
#import "components/autofill/core/browser/autofill_driver_factory.h"
#import "components/autofill/core/browser/autofill_driver_router.h"
#import "components/autofill/ios/browser/autofill_driver_ios_bridge.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebFrame;
class WebState;
}  // namespace web

namespace autofill {

class AutofillDriverIOS;

// This factory will keep the parameters needed to create an AutofillDriverIOS.
// These parameters only depend on the web_state, so there is one
// AutofillDriverIOSFactory per WebState.
// TODO: crbug.com/355907668 - Introduce AutofillClientIOS and move ownership to
// that class.
class AutofillDriverIOSFactory final
    : public AutofillDriverFactory,
      public web::WebStateUserData<AutofillDriverIOSFactory> {
 public:
  ~AutofillDriverIOSFactory() override;

  // Returns the AutofillDriverIOS for `web_frame`. Creates the driver if
  // necessary.
  AutofillDriverIOS* DriverForFrame(web::WebFrame* web_frame);

  // TODO: crbug.com/354043640 - Eliminate.
  void SetLifecycleStateAndNotifyObservers(
      AutofillDriver& driver,
      LifecycleState new_state,
      base::PassKey<AutofillDriverIOS> pass_key) {
    AutofillDriverFactory::SetLifecycleStateAndNotifyObservers(driver,
                                                               new_state);
  }

  // TODO: crbug.com/354043640 - Eliminate.
  const base::ObserverList<Observer>& observers(
      base::PassKey<AutofillDriverIOS> pass_key) {
    return AutofillDriverFactory::observers();
  }

  AutofillDriverRouter& router() { return router_; }

 private:
  friend class web::WebStateUserData<AutofillDriverIOSFactory>;

  // Creates a AutofillDriverIOSFactory that will store all the
  // needed to create a AutofillDriverIOS.
  AutofillDriverIOSFactory(web::WebState* web_state,
                           AutofillClient* client,
                           id<AutofillDriverIOSBridge> bridge,
                           const std::string& app_locale);

  raw_ptr<web::WebState> web_state_ = nullptr;
  raw_ptr<AutofillClient> client_ = nullptr;
  id<AutofillDriverIOSBridge> bridge_ = nil;
  std::string app_locale_;
  AutofillDriverRouter router_;
  WEB_STATE_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_FACTORY_H_
