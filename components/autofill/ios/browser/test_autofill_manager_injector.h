// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_TEST_AUTOFILL_MANAGER_INJECTOR_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_TEST_AUTOFILL_MANAGER_INJECTOR_H_

#include "base/scoped_observation.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"

namespace autofill {

// Upon construction, and in response to WebFrameDidBecomeAvailable, installs an
// BrowserAutofillManager of type `T`.
//
// Typical usage as a RAII type:
//
//   class MockAutofillManager : BrowserAutofillManager {
//    public:
//     MockAutofillManager(AutofillDriverIOS* driver,
//                         AutofillClient* client)
//         : BrowserAutofillManager(driver, client, "en-US",
//                                  EnableDownloadManager(true)) {}
//     MOCK_METHOD(...);
//     ...
//   };
//
//   TestAutofillManagerInjector<MockAutofillManager> injector(web_state());
//   NavigateToURL(...);
template <typename T>
class TestAutofillManagerInjector : public web::WebStateObserver {
 public:
  // Builds the managers using `T(AutofillDriverIOS*, AutofillClient*)`.
  explicit TestAutofillManagerInjector(web::WebState* web_state)
      : web_state_(web_state) {
    observation_.Observe(web_state);
    if (web::WebFrame* main_frame =
            web_state->GetWebFramesManager()->GetMainWebFrame()) {
      Inject(main_frame);
    }
  }

  ~TestAutofillManagerInjector() override = default;

  T* GetForMainFrame() {
    return GetForFrame(web_state_->GetWebFramesManager()->GetMainWebFrame());
  }

  T* GetForFrame(web::WebFrame* web_frame) {
    BrowserAutofillManager* autofill_manager =
        AutofillDriverIOS::FromWebStateAndWebFrame(web_state_, web_frame)
            ->autofill_manager();
    return static_cast<T*>(autofill_manager);
  }

 private:
  // web::WebStateObserver:
  void WebFrameDidBecomeAvailable(web::WebState* web_state,
                                  web::WebFrame* web_frame) override {
    Inject(web_frame);
  }

  void WebStateDestroyed(web::WebState* web_state) override {
    observation_.Reset();
  }

  void Inject(web::WebFrame* web_frame) {
    AutofillDriverIOS* driver =
        AutofillDriverIOS::FromWebStateAndWebFrame(web_state_, web_frame);
    AutofillClient* client = driver->client();
    driver->set_autofill_manager_for_testing(CreateManager(driver, client));
  }

  std::unique_ptr<T> CreateManager(AutofillDriverIOS* driver,
                                   AutofillClient* client) {
    return std::make_unique<T>(driver, client);
  }

  web::WebState* web_state_;
  base::ScopedObservation<web::WebState, web::WebStateObserver> observation_{
      this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_TEST_AUTOFILL_MANAGER_INJECTOR_H_
