// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_TEST_AUTOFILL_MANAGER_INJECTOR_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_TEST_AUTOFILL_MANAGER_INJECTOR_H_

#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"

namespace autofill {

// RAII type that installs new AutofillManagers of type `T`
// - in the primary main frame of the given WebContents,
// - in any frame of the WebContetns when a navigation is committed.
//
// Usage:
//
//   class MockAutofillManager : BrowserAutofillManager {
//    public:
//     MockAutofillManager(ContentAutofillDriver* driver,
//                         AutofillClient* client)
//         : BrowserAutofillManager(driver, client, "en-US",
//                                  EnableDownloadManager(true)) {}
//     MOCK_METHOD(...);
//     ...
//   };
//
//   TestAutofillManagerInjector<MockAutofillManager> injector(web_contents());
//   ui_test_utils::NavigateToURL(...);
//   injector.GetForPrimaryMainFrame()->Foo();
//
// To inject into not-yet-created WebContents, see
// TestAutofillManagerFutureInjectors.
template <typename T>
class TestAutofillManagerInjector : public content::WebContentsObserver {
 public:
  // Builds the managers using `T(ContentAutofillDriver*, AutofillClient*)`.
  explicit TestAutofillManagerInjector(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {
    Inject(web_contents->GetPrimaryMainFrame());
  }

  TestAutofillManagerInjector(const TestAutofillManagerInjector&) = delete;
  TestAutofillManagerInjector& operator=(const TestAutofillManagerInjector&) =
      delete;

  ~TestAutofillManagerInjector() override = default;

  T* GetForPrimaryMainFrame() {
    return GetForFrame(web_contents()->GetPrimaryMainFrame());
  }

  T* GetForFrame(content::RenderFrameHost* rfh) {
    ContentAutofillDriverFactory* driver_factory =
        ContentAutofillDriverFactory::FromWebContents(web_contents());
    return static_cast<T*>(
        driver_factory->DriverForFrame(rfh)->autofill_manager());
  }

 private:
  // content::WebContentsObserver:
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->IsPrerenderedPageActivation() &&
        !navigation_handle->IsSameDocument()) {
      Inject(navigation_handle->GetRenderFrameHost());
    }
  }

  void Inject(content::RenderFrameHost* rfh) {
    auto* driver_factory =
        ContentAutofillDriverFactory::FromWebContents(web_contents());
    // The ContentAutofillDriverFactory doesn't exist yet if the WebContents is
    // currently being created. Not injecting a driver in this case is correct:
    // it'll be injected on ReadyToCommitNavigation().
    if (!driver_factory)
      return;
    AutofillClient* client = driver_factory->client();
    ContentAutofillDriver* driver = driver_factory->DriverForFrame(rfh);
    driver->set_autofill_manager(CreateManager(driver, client));
  }

  std::unique_ptr<T> CreateManager(ContentAutofillDriver* driver,
                                   AutofillClient* client) {
    return std::make_unique<T>(driver, client);
  }
};

// RAII type that sets up TestAutofillManagerInjectors for every newly created
// WebContents.
//
// Usage:
//
//   TestAutofillManagerInjectors<MockAutofillManager> injectors;
//   NavigateParams params(...);
//   params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
//   ui_test_utils::NavigateToURL(&params);
//   injectors[0].GetForPrimaryMainFrame()->Foo();
template <typename T>
class TestAutofillManagerFutureInjectors {
 public:
  TestAutofillManagerFutureInjectors() = default;
  TestAutofillManagerFutureInjectors(
      const TestAutofillManagerFutureInjectors&) = delete;
  TestAutofillManagerFutureInjectors& operator=(
      const TestAutofillManagerFutureInjectors&) = delete;
  ~TestAutofillManagerFutureInjectors() = default;

  bool empty() const { return injectors_.empty(); }
  size_t size() const { return injectors_.size(); }

  TestAutofillManagerInjector<T>& operator[](size_t i) {
    return *injectors_[i];
  }

 private:
  // Holds the injectors created by the lambda below.
  std::vector<std::unique_ptr<TestAutofillManagerInjector<T>>> injectors_;

  // Registers the lambda for the lifetime of `subscription_`.
  base::CallbackListSubscription subscription_ =
      content::RegisterWebContentsCreationCallback(base::BindRepeating(
          [](TestAutofillManagerFutureInjectors* self,
             content::WebContents* web_contents) {
            self->injectors_.push_back(
                std::make_unique<TestAutofillManagerInjector<T>>(web_contents));
          },
          base::Unretained(this)));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_TEST_AUTOFILL_MANAGER_INJECTOR_H_
