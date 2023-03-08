// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_TEST_AUTOFILL_MANAGER_INJECTOR_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_TEST_AUTOFILL_MANAGER_INJECTOR_H_

#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
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
// alive at a time. It must not be created before a TestAutofillClientInjector.
// These conditions are CHECKed.
//
// Usage:
//
//   class AutofillFooTest : public ... {
//    public:
//     class MockAutofillManager : BrowserAutofillManager {
//      public:
//       MockAutofillManager(ContentAutofillDriver* driver,
//                           AutofillClient* client)
//           : BrowserAutofillManager(driver, client, "en-US") {}
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
class TestAutofillManagerInjector : TestAutofillManagerInjectorBase {
 public:
  static_assert(std::is_base_of_v<AutofillManager, T>);

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
    ~Injector() override = default;

    void RenderFrameCreated(content::RenderFrameHost* rfh) override {
      if (observation_.IsObserving()) {
        return;
      }
      if (auto* client =
              ContentAutofillClient::FromWebContents(web_contents())) {
        observation_.Observe(client->GetAutofillDriverFactory());
      }
    }

    void OnContentAutofillDriverFactoryDestroyed(
        ContentAutofillDriverFactory& factory) override {
      observation_.Reset();
    }

    // Replaces the just created `driver`'s manager with a new test manager.
    void OnContentAutofillDriverCreated(
        ContentAutofillDriverFactory& factory,
        ContentAutofillDriver& driver) override {
      std::unique_ptr<T> new_manager = CreateManager(&driver);
      owner_->managers_[driver.render_frame_host()] = new_manager.get();
      driver.set_autofill_manager(std::move(new_manager));
    }

    void OnContentAutofillDriverWillBeDeleted(
        ContentAutofillDriverFactory& factory,
        ContentAutofillDriver& driver) override {
      owner_->managers_.erase(driver.render_frame_host());
    }

   private:
    std::unique_ptr<T> CreateManager(ContentAutofillDriver* driver) {
      auto* client = ContentAutofillClient::FromWebContents(web_contents());
      DCHECK(client);
      return std::make_unique<T>(driver, client);
    }

    raw_ptr<TestAutofillManagerInjector> owner_;
    base::ScopedObservation<ContentAutofillDriverFactory,
                            ContentAutofillDriverFactory::Observer>
        observation_{this};
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
