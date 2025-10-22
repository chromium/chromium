// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_DELETER_ANDROID_H_
#define COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_DELETER_ANDROID_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/browser_binding/browser_bound_key_deleter.h"

namespace webauthn {
class InternalAuthenticator;
}
namespace payments {

class BrowserBoundKeyStore;
struct BrowserBoundKeyMetadata;
class PasskeyBrowserBinder;
class WebPaymentsWebDataService;

class BrowserBoundKeyDeleterAndroid : public BrowserBoundKeyDeleter {
 public:
  explicit BrowserBoundKeyDeleterAndroid(
      scoped_refptr<WebPaymentsWebDataService> web_data_service,
      scoped_refptr<BrowserBoundKeyStore> browser_bound_key_store);

  ~BrowserBoundKeyDeleterAndroid() override;

  // BrowserBoundKeyDeleter:
  void RemoveInvalidBBKs() override;

  // Sets an InternalAuthenticator to be used for testing. If this is not set, a
  // new InternalAuthenticator will be created in `RemoveInvalidBBKs()`.
  // Note that when calling `RemoveInvalidBBKs()`, this test authenticator will
  // be moved.
  void SetInternalAuthenticatorForTesting(
      std::unique_ptr<webauthn::InternalAuthenticator> authenticator);

  // Sets a PasskeyBrowserBinder to be used for testing. If this is not set, a
  // new PasskeyBrowserBinder will be created in `RemoveInvalidBBKs()`.
  // Note that when calling `RemoveInvalidBBKs()`, this test binder will be
  // moved.
  void SetPasskeyBrowserBinderForTesting(
      std::unique_ptr<PasskeyBrowserBinder> passkey_browser_binder);

 private:
  void FilterAndDeleteInvalidBBKs(
      std::unique_ptr<webauthn::InternalAuthenticator> authenticator,
      std::unique_ptr<PasskeyBrowserBinder> passkey_browser_binder,
      std::vector<BrowserBoundKeyMetadata> browser_bound_keys);

  scoped_refptr<WebPaymentsWebDataService> web_data_service_;
  scoped_refptr<BrowserBoundKeyStore> browser_bound_key_store_;

  std::unique_ptr<webauthn::InternalAuthenticator> authenticator_for_testing_;
  std::unique_ptr<PasskeyBrowserBinder> passkey_browser_binder_for_testing_;

  base::WeakPtrFactory<BrowserBoundKeyDeleterAndroid> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_DELETER_ANDROID_H_
