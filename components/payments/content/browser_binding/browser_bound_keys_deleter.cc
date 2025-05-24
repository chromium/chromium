// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/browser_binding/browser_bound_keys_deleter.h"

#include "components/payments/content/browser_binding/passkey_browser_binder.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/webauthn/android/internal_authenticator_android.h"
#include "third_party/blink/public/common/features.h"

namespace payments {

BrowserBoundKeyDeleter::BrowserBoundKeyDeleter(
    scoped_refptr<PaymentManifestWebDataService> web_data_service)
    : web_data_service_(web_data_service) {}

BrowserBoundKeyDeleter::~BrowserBoundKeyDeleter() = default;

void BrowserBoundKeyDeleter::RemoveInvalidBBKs() {
  if (!base::FeatureList::IsEnabled(
          blink::features::kSecurePaymentConfirmationBrowserBoundKeys)) {
    return;
  }
  auto authenticator = std::make_unique<webauthn::InternalAuthenticatorAndroid>(
      /*render_frame_host=*/nullptr);
  if (!authenticator->IsGetMatchingCredentialIdsSupported()) {
    // SPC (on Android) requires GetMatchingCredentialIds, so BBKs are not
    // relevant when this API is not supported.
    return;
  }
  scoped_refptr<BrowserBoundKeyStore> bbk_key_store =
      GetBrowserBoundKeyStoreInstance();
  CHECK(bbk_key_store);
  auto passkey_browser_binder =
      std::make_unique<PasskeyBrowserBinder>(bbk_key_store, web_data_service_);
  passkey_browser_binder->DeleteAllUnknownBrowserBoundKeys(
      base::BindRepeating(
          &webauthn::InternalAuthenticatorAndroid::GetMatchingCredentialIds,
          // The authenticator will be destroyed once this callback is no longer
          // referenced.
          base::Owned(std::move(authenticator))),
      base::BindOnce(
          [](std::unique_ptr<PasskeyBrowserBinder> passkey_browser_binder) {
            // This callback runs after BBK metadata has been deleted.
            // Destroy the unique_ptr objects: Reset here explicitly for
            // emphasis. Note the unique_ptr object would be reset regardless
            // by going out of scope.
            passkey_browser_binder.reset();
          },
          std::move(passkey_browser_binder)));
  // Don't access authenticator nor passkey_browser_binder after this point.
}

}  // namespace payments
