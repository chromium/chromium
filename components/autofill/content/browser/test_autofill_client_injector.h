// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_TEST_AUTOFILL_CLIENT_INJECTOR_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_TEST_AUTOFILL_CLIENT_INJECTOR_H_

#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/content/browser/content_autofill_driver_factory_test_api.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"

namespace autofill {

// Asserts that at construction time, no other TestAutofillClientInjector *and*
// no TestAutofillManagerInjector are alive.
class TestAutofillClientInjectorBase {
 public:
  static bool some_instance_is_alive() { return num_instances_ > 0; }

  TestAutofillClientInjectorBase(const TestAutofillClientInjectorBase&) =
      delete;
  TestAutofillClientInjectorBase& operator=(
      const TestAutofillClientInjectorBase&) = delete;

 protected:
  TestAutofillClientInjectorBase();
  ~TestAutofillClientInjectorBase();

 private:
  static size_t num_instances_;
};

// RAII type that installs new AutofillClients of type `T` in all newly created
// WebContents.
//
// To prevent hard-to-find bugs, only one TestAutofillClientInjector may be
// alive at a time, and that instance must not be created after a
// TestAutofillManagerInjector (note: *Manager*Injector). These conditions are
// CHECKed.
//
// Usage:
//
//   class AutofillFooTest : public ... {
//    public:
//     class MockAutofillClient : ChromeAutofillClient {
//      public:
//       MockAutofillClient(content::WebContents* web_contents)
//           : ChromeAutofillClient(web_contents) {}
//       MOCK_METHOD(...);
//       ...
//     };
//
//     MockAutofillClient* autofill_client(content::WebContents* web_contents) {
//       return autofill_client_injector_[web_contents];
//     }
//
//    private:
//     TestAutofillClientInjector<MockAutofillClient> autofill_client_injector_;
//   };
template <typename T>
class TestAutofillClientInjector : public TestAutofillClientInjectorBase {
 public:
  TestAutofillClientInjector() = default;
  TestAutofillClientInjector(const TestAutofillClientInjector&) = delete;
  TestAutofillClientInjector& operator=(const TestAutofillClientInjector&) =
      delete;
  ~TestAutofillClientInjector() = default;

  T* operator[](content::WebContents* web_contents) const {
    auto it = clients_.find(web_contents);
    return it != clients_.end() ? it->second : nullptr;
  }

 private:
  void InjectClient(content::WebContents* web_contents) {
    auto client = std::make_unique<T>(web_contents);
    if (auto* driver_factory =
            ContentAutofillDriverFactory::FromWebContents(web_contents)) {
      ContentAutofillDriverFactoryTestApi(driver_factory)
          .set_client(client.get());
    }
    clients_[web_contents] = client.get();
    web_contents->SetUserData(T::UserDataKey(), std::move(client));
  }

  std::map<content::WebContents*, T*> clients_;

  // Registers the lambda for the lifetime of `subscription_`.
  base::CallbackListSubscription subscription_ =
      content::RegisterWebContentsCreationCallback(
          base::BindRepeating(&TestAutofillClientInjector::InjectClient,
                              base::Unretained(this)));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_TEST_AUTOFILL_MANAGER_INJECTOR_H_
