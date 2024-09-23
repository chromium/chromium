// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_TEST_AUTOFILL_DRIVER_INJECTOR_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_TEST_AUTOFILL_DRIVER_INJECTOR_H_

#include <concepts>
#include <map>
#include <memory>

#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/content/browser/content_autofill_driver_factory_test_api.h"
#include "components/autofill/content/browser/content_autofill_driver_test_api.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"

namespace autofill {

// Asserts that at construction time, no other TestAutofillDriverInjector and no
// TestAutofillManagerInjector are alive.
class TestAutofillDriverInjectorBase {
 public:
  static bool some_instance_is_alive() { return num_instances_ > 0; }

  TestAutofillDriverInjectorBase(const TestAutofillDriverInjectorBase&) =
      delete;
  TestAutofillDriverInjectorBase& operator=(
      const TestAutofillDriverInjectorBase&) = delete;

 protected:
  TestAutofillDriverInjectorBase();
  ~TestAutofillDriverInjectorBase();

 private:
  static size_t num_instances_;
};

// RAII type that installs new AutofillDrivers of type `T` in all newly
// navigated frames in all newly created WebContents.
//
// The injector only injects an AutofillDriver if a driver would also be created
// Especially in unit tests it may be necessary to do a navigation to create the
// driver, for example with
//   NavigateAndCommit(GURL("about:blank"))
// or force-create the driver manually with
//   client->GetAutofillDriverFactory().DriverForFrame(rfh).
//
// The driver's AutofillManager is a fresh BrowserAutofillManager.
//
// To prevent hard-to-find bugs, only one TestAutofillDriverInjector may be
// alive at a time. It must not be created before a TestAutofillClientInjector
// and not after a TestAutofillManagerInjector. These conditions are CHECKed.
//
// Usage:
//
//   class AutofillFooTest : public ... {
//    public:
//     TestAutofillDriver* autofill_driver(content::RenderFrameHost* rfh) {
//       return autofill_driver_injector_[rfh];
//     }
//
//    private:
//     TestAutofillDriverInjector<TestAutofillDriver> autofill_driver_injector_;
//   };
template <std::derived_from<ContentAutofillDriver> T>
class TestAutofillDriverInjector : public TestAutofillDriverInjectorBase {
 public:
  TestAutofillDriverInjector() = default;
  TestAutofillDriverInjector(const TestAutofillDriverInjector&) = delete;
  TestAutofillDriverInjector& operator=(const TestAutofillDriverInjector&) =
      delete;
  ~TestAutofillDriverInjector() = default;

  T* operator[](content::WebContents* web_contents) const {
    return (*this)[web_contents->GetPrimaryMainFrame()];
  }

  T* operator[](content::RenderFrameHost* rfh) const {
    auto it = drivers_.find(rfh);
    return it != drivers_.end() ? it->second : nullptr;
  }

 private:
  // Replaces every newly created production-code ContentAutofillDriver for the
  // given WebContents with a test driver created by
  // `T(content::RenderFrameHost*, AutofillDriverRouter*)` and sets its
  // AutofillManager to a new `BrowserAutofillManager` with locale "en-US".
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
    Injector(TestAutofillDriverInjector* owner,
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
      test_api(*factory_).AddObserverAtIndex(this, 0);
    }

    void OnContentAutofillDriverFactoryDestroyed(
        ContentAutofillDriverFactory& factory) override {
      if (factory_) {
        factory_->RemoveObserver(this);
        factory_ = nullptr;
      }
    }

    // Replaces the just created `driver` with a new test driver.
    void OnContentAutofillDriverCreated(
        ContentAutofillDriverFactory& factory,
        ContentAutofillDriver& driver) override {
      content::RenderFrameHost* rfh = driver.render_frame_host();
      std::unique_ptr<T> new_driver = std::make_unique<T>(rfh, &factory);
      owner_->drivers_[rfh] = new_driver.get();
      // This is spectacularly hacky as it relies on an implementation detail of
      // ContentAutofillDriverFactory::DriverForFrame(), which fires this event:
      //
      // DriverForFrame() has just created `driver` and still holds a reference
      // to the std::unique_ptr<ContentAutofillDriver> that owns `driver`. By
      // calling ExchangeDriver(), we mutate that reference. This is safe but it
      // is "safe" because ExchangeDriver() doesn't invalidate references.
      test_api(factory).ExchangeDriver(rfh, std::move(new_driver));
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
          owner_->drivers_.erase(driver.render_frame_host());
          break;
      }
    }

   private:
    raw_ptr<TestAutofillDriverInjector> owner_;

    // Observed source. We can't use a ScopedObservation because we use
    // ContentAutofillDriverFactoryTestApi::AddObserverAtIndex() instead of
    // ContentAutofillDriverFactory::AddObserver().
    raw_ptr<ContentAutofillDriverFactory> factory_ = nullptr;
  };

  void ObserveWebContentsAndInjectDriver(content::WebContents* web_contents) {
    injectors_.push_back(std::make_unique<Injector>(this, web_contents));
  }

  std::vector<std::unique_ptr<Injector>> injectors_;
  std::map<content::RenderFrameHost*, T*> drivers_;

  // Registers the lambda for the lifetime of `subscription_`.
  base::CallbackListSubscription subscription_ =
      content::RegisterWebContentsCreationCallback(base::BindRepeating(
          &TestAutofillDriverInjector::ObserveWebContentsAndInjectDriver,
          base::Unretained(this)));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_TEST_AUTOFILL_DRIVER_INJECTOR_H_
