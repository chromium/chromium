// Copyright 2022 The Chromium Authors. All rights reserved.
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

namespace autofill {

// Upon construction, and in response to ReadyToCommitNavigation, installs an
// AutofillManager of type `T`.
//
// Typical usage as a RAII type:
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
//   NavigateToURL(...);
template <typename T>
class TestAutofillManagerInjector : public content::WebContentsObserver {
 public:
  // Builds the managers using `T(ContentAutofillDriver*, AutofillClient*)`.
  explicit TestAutofillManagerInjector(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {
    Inject(web_contents->GetPrimaryMainFrame());
  }

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
    ContentAutofillDriverFactory* driver_factory =
        ContentAutofillDriverFactory::FromWebContents(web_contents());
    AutofillClient* client = driver_factory->client();
    ContentAutofillDriver* driver = driver_factory->DriverForFrame(rfh);
    driver->set_autofill_manager(CreateManager(driver, client));
  }

  std::unique_ptr<T> CreateManager(ContentAutofillDriver* driver,
                                   AutofillClient* client) {
    return std::make_unique<T>(driver, client);
  }
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_TEST_AUTOFILL_MANAGER_INJECTOR_H_
