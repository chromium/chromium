// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_TEST_AUTOFILL_MANAGER_INJECTOR_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_TEST_AUTOFILL_MANAGER_INJECTOR_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/autofill_client.h"
#import "components/autofill/core/browser/autofill_driver_test_api.h"
#import "components/autofill/core/browser/autofill_manager_test_api.h"
#import "components/autofill/core/browser/browser_autofill_manager.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_driver_ios_test_api.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"

namespace autofill {

// Upon construction, and in response to WebFrameBecameAvailable, installs an
// BrowserAutofillManager of type `T` in the main frame of the given `web_state`
// and all subsequently created frames of the `web_state`.
//
// Typical usage as a RAII type:
//
//   class MockAutofillManager : BrowserAutofillManager {
//    public:
//     explicit MockAutofillManager(AutofillDriverIOS* driver)
//         : BrowserAutofillManager(driver, "en-US") {}
//     MOCK_METHOD(...);
//     ...
//   };
//
//   TestAutofillManagerInjector<MockAutofillManager> injector(web_state());
//   NavigateToURL(...);
template <typename T>
class TestAutofillManagerInjector : public web::WebFramesManager::Observer,
                                    public web::WebStateObserver {
 public:
  // Builds the managers using `T(AutofillDriverIOS*, AutofillClient*)`.
  explicit TestAutofillManagerInjector(web::WebState* web_state)
      : web_state_(web_state) {
    web_state_observation_.Observe(web_state);
    web::WebFramesManager* frames_manager =
        AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(
            web_state);
    frames_manager_observation_.Observe(frames_manager);
    if (web::WebFrame* main_frame = frames_manager->GetMainWebFrame()) {
      Inject(main_frame);
    }
  }

  ~TestAutofillManagerInjector() override = default;

  T* GetForMainFrame() {
    web::WebFramesManager* frames_manager =
        AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(
            web_state_);
    return GetForFrame(frames_manager->GetMainWebFrame());
  }

  T* GetForFrame(web::WebFrame* web_frame) {
    BrowserAutofillManager& autofill_manager =
        AutofillDriverIOS::FromWebStateAndWebFrame(web_state_, web_frame)
            ->GetAutofillManager();
    return &static_cast<T&>(autofill_manager);
  }

 private:
  // web::WebFramesManager::Observer:
  void WebFrameBecameAvailable(web::WebFramesManager* web_frames_manager,
                               web::WebFrame* web_frame) override {
    Inject(web_frame);
  }

  // web::WebStateObserver:
  void WebStateDestroyed(web::WebState* web_state) override {
    web_state_observation_.Reset();
    frames_manager_observation_.Reset();
  }

  void Inject(web::WebFrame* web_frame) {
    AutofillDriverIOS* driver =
        AutofillDriverIOS::FromWebStateAndWebFrame(web_state_, web_frame);
    // There's no guarantee that the injector's WebFrameBecameAvailable() event
    // handler comes before other handlers. These other handlers may have
    // touched `driver` already. In particular, they may have registered
    // AutofillManager::Observers. We need to notify those observers that the
    // `manager` is about to die so they can clean up their observations.
    //
    // TODO: crbug.com/354043640 - Observe
    // AutofillDriverFactory::OnAutofillDriverCreated() instead of
    // WebFrameBecameAvailable(), and use AddObserverAtIndex().
    using LifecycleState = AutofillDriverIOS::LifecycleState;
    LifecycleState old_state = driver->GetLifecycleState();
    CHECK_EQ(old_state, LifecycleState::kActive);
    AutofillManager& manager = driver->GetAutofillManager();
    for (auto& observer : test_api(manager).observers()) {
      observer.OnAutofillManagerStateChanged(manager, old_state,
                                             LifecycleState::kPendingDeletion);
    }

    test_api(*driver).SetAutofillManager(std::make_unique<T>(driver));
  }

  raw_ptr<web::WebState> web_state_;
  base::ScopedObservation<web::WebFramesManager,
                          web::WebFramesManager::Observer>
      frames_manager_observation_{this};
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_TEST_AUTOFILL_MANAGER_INJECTOR_H_
