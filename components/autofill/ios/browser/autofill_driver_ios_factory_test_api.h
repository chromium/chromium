// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_FACTORY_TEST_API_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_FACTORY_TEST_API_H_

#include "components/autofill/core/browser/autofill_driver_test_api.h"
#include "components/autofill/ios/browser/autofill_driver_ios_factory.h"

namespace web {
class WebFrame;
}

namespace autofill {

// Exposes some testing operations for AutofillDriverIOSFactory.
class AutofillDriverIOSFactoryTestApi : public AutofillDriverFactoryTestApi {
 public:
  explicit AutofillDriverIOSFactoryTestApi(AutofillDriverIOSFactory* factory)
      : AutofillDriverFactoryTestApi(factory) {}

  size_t num_drivers() { return factory().driver_map_.size(); }

  AutofillDriverIOS* DriverForFrame(web::WebFrame* web_frame) {
    return factory().DriverForFrame(web_frame);
  }

 private:
  AutofillDriverIOSFactory& factory() {
    return static_cast<AutofillDriverIOSFactory&>(*factory_);
  }
};

inline AutofillDriverIOSFactoryTestApi test_api(
    AutofillDriverIOSFactory& factory) {
  return AutofillDriverIOSFactoryTestApi(&factory);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_FACTORY_TEST_API_H_
