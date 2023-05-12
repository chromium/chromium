// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/ios/browser/autofill_driver_ios_factory.h"

#include "components/autofill/ios/browser/autofill_driver_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {

AutofillDriverIOSFactory::AutofillDriverIOSFactory(
    web::WebState* web_state,
    AutofillClient* client,
    id<AutofillDriverIOSBridge> bridge,
    const std::string& app_locale)
    : web_state_(web_state),
      client_(client),
      bridge_(bridge),
      app_locale_(app_locale) {}

AutofillDriverIOSFactory::~AutofillDriverIOSFactory() = default;

AutofillDriverIOS* AutofillDriverIOSFactory::DriverForFrame(
    web::WebFrame* web_frame) {
  if (AutofillDriverIOS* driver = AutofillDriverIOS::FromWebFrame(web_frame)) {
    return driver;
  }
  std::unique_ptr<AutofillDriverIOS> driver =
      base::WrapUnique(new AutofillDriverIOS(web_state_, web_frame, client_,
                                             bridge_, app_locale_));
  auto* raw_driver = driver.get();
  web_frame->SetUserData(AutofillDriverIOS::UserDataKey(), std::move(driver));
  return raw_driver;
}

WEB_STATE_USER_DATA_KEY_IMPL(AutofillDriverIOSFactory)

}  //  namespace autofill
