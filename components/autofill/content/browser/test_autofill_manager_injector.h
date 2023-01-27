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

// RAII type that installs new AutofillManagers of type `T` in all newly
// navigated frames in all newly created WebContents.
//
// To prevent hard-to-find bugs, only one TestAutofillManagerInjector may be
// alive at a time. This condition is CHECKed.
//
// Usage:
//
//   class AutofillFooTest : public ... {
//    public:
//     class MockAutofillManager : BrowserAutofillManager {
//      public:
//       MockAutofillManager(ContentAutofillDriver* driver,
//                           AutofillClient* client)
//           : BrowserAutofillManager(driver, client, "en-US",
//                                    EnableDownloadManager(true)) {}
//       MOCK_METHOD(...);
//       ...
//     };
//
//     MockAutofillManager* autofill_manager(content::RenderFrameHost* rfh) {
//       return autofill_manager_injector_[rfh];
//     }
//
//    private:
//     TestAutofillManagerInjector<MockAutofillManager>
//     autofill_manager_injector_;
//   };
template <typename T>
class TestAutofillManagerInjector : TestAutofillManagerInjectorBase {
 public:
  TestAutofillManagerInjector() = default;
  TestAutofillManagerInjector(const TestAutofillManagerInjector&) = delete;
  TestAutofillManagerInjector& operator=(const TestAutofillManagerInjector&) =
      delete;
  ~TestAutofillManagerInjector() = default;

  T* operator[](content::WebContents* web_contents) const {
    return (*this)[web_contents->GetPrimaryMainFrame()];
  }

  T* operator[](content::RenderFrameHost* rfh) const {
    auto it = managers_.find(rfh);
    return it != managers_.end() ? it->second : nullptr;
  }

 private:
  // Creates an AutofillManager using `T(ContentAutofillDriver*,
  // AutofillClient*)` for every navigated frame in a given `WebContents`.
  class Injector : public content::WebContentsObserver {
   public:
    Injector(TestAutofillManagerInjector* owner,
             content::WebContents* web_contents)
        : WebContentsObserver(web_contents), owner_(owner) {
      InjectManager(web_contents->GetPrimaryMainFrame());
    }
    Injector(const Injector&) = delete;
    Injector& operator=(const Injector&) = delete;
    ~Injector() override = default;

   private:
    // content::WebContentsObserver:
    void ReadyToCommitNavigation(
        content::NavigationHandle* navigation_handle) override {
      if (!navigation_handle->IsPrerenderedPageActivation() &&
          !navigation_handle->IsSameDocument()) {
        InjectManager(navigation_handle->GetRenderFrameHost());
      }
    }

    void RenderFrameDeleted(content::RenderFrameHost* rfh) override {
      owner_->managers_.erase(rfh);
    }

    void InjectManager(content::RenderFrameHost* rfh) {
      if ((*owner_)[rfh]) {
        // AutofillManager was already injected.
        return;
      }
      auto* driver_factory =
          ContentAutofillDriverFactory::FromWebContents(web_contents());
      // The ContentAutofillDriverFactory doesn't exist yet if the WebContents
      // is currently being created. Not injecting a driver in this case is
      // correct: it'll be injected on ReadyToCommitNavigation().
      if (!driver_factory) {
        return;
      }
      AutofillClient* client = driver_factory->client();
      ContentAutofillDriver* driver = driver_factory->DriverForFrame(rfh);
      auto manager = std::make_unique<T>(driver, client);
      owner_->managers_[rfh] = manager.get();
      driver->set_autofill_manager(std::move(manager));
    }

    raw_ptr<TestAutofillManagerInjector> owner_;
  };

  void ObserveWebContentsAndInjectManager(content::WebContents* web_contents) {
    injectors_.push_back(std::make_unique<Injector>(this, web_contents));
  }

  std::vector<std::unique_ptr<Injector>> injectors_;
  std::map<content::RenderFrameHost*, T*> managers_;

  // Registers the lambda for the lifetime of `subscription_`.
  base::CallbackListSubscription subscription_ =
      content::RegisterWebContentsCreationCallback(base::BindRepeating(
          &TestAutofillManagerInjector::ObserveWebContentsAndInjectManager,
          base::Unretained(this)));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_TEST_AUTOFILL_MANAGER_INJECTOR_H_
