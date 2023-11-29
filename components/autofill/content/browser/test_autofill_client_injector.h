// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_TEST_AUTOFILL_CLIENT_INJECTOR_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_TEST_AUTOFILL_CLIENT_INJECTOR_H_

#include <concepts>

#include "components/autofill/content/browser/content_autofill_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"

namespace autofill {

// Asserts that at construction time, no other TestAutofillClientInjector, no
// other TestAutofillDriverInjector, and no TestAutofillManagerInjector are
// alive.
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
// This happens *before* the production-code ContentAutofillClient is
// associated. It thus avoids dangling pointers to the production-code
// ContentAutofillClient.
//
// To prevent hard-to-find bugs, only one TestAutofillClientInjector may be
// alive at a time. It is compatible with TestAutofillDriverInjector and/or
// TestAutofillManagerInjector, but the client injector must be created first.
// These conditions are CHECKed.
//
// Usage:
//
//   class AutofillFooTest : public ... {
//    public:
//     TestContentAutofillClient* autofill_client(
//         content::WebContents* web_contents) {
//       return autofill_client_injector_[web_contents];
//     }
//
//    private:
//     TestAutofillClientInjector<TestContentAutofillClient>
//         autofill_client_injector_;
//   };
template <std::derived_from<ContentAutofillClient> T>
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

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_TEST_AUTOFILL_CLIENT_INJECTOR_H_
