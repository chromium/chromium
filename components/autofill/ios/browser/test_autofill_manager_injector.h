// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_TEST_AUTOFILL_MANAGER_INJECTOR_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_TEST_AUTOFILL_MANAGER_INJECTOR_H_

#import <ranges>

#import "base/check_deref.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/raw_ref.h"
#import "base/scoped_observation.h"
#import "components/autofill/core/browser/foundations/autofill_client.h"
#import "components/autofill/core/browser/foundations/autofill_driver_test_api.h"
#import "components/autofill/core/browser/foundations/autofill_manager_test_api.h"
#import "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#import "components/autofill/ios/browser/autofill_client_ios.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/autofill_driver_ios_test_api.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"

namespace autofill {

// Asserts that at construction time, no other TestAutofillManagerInjector is
// alive.
class TestAutofillManagerInjectorBase {
 public:
  static bool some_instance_is_alive() { return num_instances_ > 0; }

  TestAutofillManagerInjectorBase(const TestAutofillManagerInjectorBase&) =
      delete;
  TestAutofillManagerInjectorBase& operator=(
      const TestAutofillManagerInjectorBase&) = delete;

 protected:
  TestAutofillManagerInjectorBase();
  ~TestAutofillManagerInjectorBase();

 private:
  static size_t num_instances_;
};

// Upon construction, and in response to WebFrameBecameAvailable, installs an
// BrowserAutofillManager of type `T` in the main frame of the given `web_state`
// and all subsequently created frames of the `web_state`.
//
// Typical usage as a RAII type:
//
//   class MockAutofillManager : BrowserAutofillManager {
//    public:
//     explicit MockAutofillManager(AutofillDriverIOS* driver)
//         : BrowserAutofillManager(driver) {}
//     MOCK_METHOD(...);
//     ...
//   };
//
//   TestAutofillManagerInjector<MockAutofillManager> injector(web_state());
//   NavigateToURL(...);
template <typename T>
  requires(std::derived_from<T, AutofillManager>)
class TestAutofillManagerInjector : public AutofillDriverIOSFactory::Observer,
                                    public TestAutofillManagerInjectorBase {
 public:
  explicit TestAutofillManagerInjector(web::WebState* web_state)
      : web_state_(web_state) {
    AutofillClientIOS& client =
        CHECK_DEREF(AutofillClientIOS::FromWebState(web_state));
    factory_ = &(client.GetAutofillDriverFactory());
    test_api(*factory_).AddObserverAtIndex(this, 0);

    web::WebFramesManager& frames_manager = CHECK_DEREF(
        AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(
            web_state_));
    if (web::WebFrame* main_frame = frames_manager.GetMainWebFrame()) {
      Inject(CHECK_DEREF(factory_->DriverForFrame(main_frame)));
    }
  }

  ~TestAutofillManagerInjector() override {
    if (factory_) {
      factory_->RemoveObserver(this);
    }
  }

  T* GetForMainFrame() {
    web::WebFramesManager* frames_manager =
        AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(
            web_state_);
    return GetForFrame(frames_manager->GetMainWebFrame());
  }

  T* GetForFrame(web::WebFrame* web_frame) {
    return &static_cast<T&>(
        factory_->DriverForFrame(web_frame)->GetAutofillManager());
  }

 private:
  // AutofillDriverIOSFactory::Observer:
  void OnAutofillDriverIOSFactoryDestroyed(
      AutofillDriverIOSFactory& factory) override {
    CHECK_EQ(&factory, factory_);
    factory_->RemoveObserver(this);
    factory_ = nullptr;
  }

  void OnAutofillDriverIOSCreated(AutofillDriverIOSFactory& factory,
                                  AutofillDriverIOS& driver) override {
    Inject(driver);
  }

  void Inject(AutofillDriverIOS& driver) {
    test_api(driver).SetAutofillManager(std::make_unique<T>(&driver));
  }

  raw_ptr<web::WebState, DanglingUntriaged> web_state_;
  // Non-null until OnAutofillDriverIOSFactoryDestroyed().
  raw_ptr<AutofillDriverIOSFactory> factory_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_TEST_AUTOFILL_MANAGER_INJECTOR_H_
