// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_TEST_AUTOFILL_MANAGER_INJECTOR_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_TEST_AUTOFILL_MANAGER_INJECTOR_H_

#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/content/browser/content_autofill_driver_factory_test_api.h"
#include "components/autofill/content/browser/content_autofill_driver_test_api.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/core/browser/autofill_manager_test_api.h"
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
// The injector only injects an AutofillManager if a driver is created.
// Especially in unit tests it may be necessary to do a navigation to create the
// driver, for example with
//   NavigateAndCommit(GURL("about:blank"))
// or force-create the driver manually with
//   client->GetAutofillDriverFactory().DriverForFrame(rfh).
//
// To prevent hard-to-find bugs, only one TestAutofillManagerInjector may be
// alive at a time. It must not be created before a TestAutofillClientInjector.
// These conditions are CHECKed.
//
// Usage:
//
//   class AutofillFooTest : public ... {
//    public:
//     class MockAutofillManager : BrowserAutofillManager {
//      public:
//       explicit MockAutofillManager(ContentAutofillDriver* driver)
//           : BrowserAutofillManager(driver, "en-US") {}
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
//         autofill_manager_injector_;
//   };
template <typename T>
  requires(std::derived_from<T, AutofillManager>)
class TestAutofillManagerInjector : public TestAutofillManagerInjectorBase {
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
  // Creates an AutofillManager using `T(ContentAutofillDriver*)` for every
  // navigated frame in a given `WebContents`.
  //
  // One challenge is that the ContentAutofillClient may not exist yet at the
  // time the Injector is created. (Because TabHelpers::AttachTabHelpers() is
  // run later.)
  //
  // We therefore defer registering the ContentAutofillDriverFactory::Observer
  // until the first RenderFrameCreated() event. This event comes late enough
  // that ContentAutofillClient has been created but no ContentAutofillDriver
  // has been created yet.
  class Injector : public content::WebContentsObserver,
                   public ContentAutofillDriverFactory::Observer {
   public:
    Injector(TestAutofillManagerInjector* owner,
             content::WebContents* web_contents)
        : WebContentsObserver(web_contents), owner_(owner) {}
    Injector(const Injector&) = delete;
    Injector& operator=(const Injector&) = delete;
    ~Injector() override {
      if (factory_) {
        factory_->RemoveObserver(this);
      }
    }

    void RenderFrameCreated(content::RenderFrameHost* rfh) override {
      if (factory_) {
        return;
      }
      auto* client = ContentAutofillClient::FromWebContents(web_contents());
      if (!client) {
        return;
      }
      factory_ = &client->GetAutofillDriverFactory();
      // The injectors' observers should come first so that production-code
      // observers affect the injected objects.
      // The AutofillManager injector should come right after the
      // ContentAutofillDriver injector, if one exists.
      test_api(*factory_).AddObserverAtIndex(
          this,
          TestAutofillDriverInjectorBase::some_instance_is_alive() ? 1 : 0);
    }

    void OnContentAutofillDriverFactoryDestroyed(
        ContentAutofillDriverFactory& factory) override {
      if (factory_) {
        factory_->RemoveObserver(this);
        factory_ = nullptr;
      }
    }

    // Replaces the just created `driver`'s manager with a new test manager.
    void OnContentAutofillDriverCreated(
        ContentAutofillDriverFactory& factory,
        ContentAutofillDriver& driver) override {
      auto new_manager = std::make_unique<T>(&driver);
      owner_->managers_[driver.render_frame_host()] = new_manager.get();
      test_api(driver).set_autofill_manager(std::move(new_manager));
    }

    void OnContentAutofillDriverStateChanged(
        ContentAutofillDriverFactory& factory,
        ContentAutofillDriver& driver,
        AutofillDriver::LifecycleState old_state,
        AutofillDriver::LifecycleState new_state) override {
      switch (new_state) {
        case AutofillDriver::LifecycleState::kInactive:
        case AutofillDriver::LifecycleState::kActive:
        case AutofillDriver::LifecycleState::kPendingReset:
          break;
        case AutofillDriver::LifecycleState::kPendingDeletion:
          owner_->managers_.erase(driver.render_frame_host());
          break;
      }
    }

   private:
    raw_ptr<TestAutofillManagerInjector> owner_;

    // Observed source. We can't use a ScopedObservation because we use
    // ContentAutofillDriverFactoryTestApi::AddObserverAtIndex() instead of
    // ContentAutofillDriverFactory::AddObserver().
    raw_ptr<ContentAutofillDriverFactory> factory_ = nullptr;
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
