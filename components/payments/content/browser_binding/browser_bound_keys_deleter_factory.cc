// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/browser_binding/browser_bound_keys_deleter_factory.h"

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/payments/content/browser_binding/browser_bound_key_store_android.h"
#include "components/payments/content/browser_binding/passkey_browser_binder.h"
#include "components/webauthn/android/internal_authenticator_android.h"
#include "components/webdata_services/web_data_service_wrapper.h"
#include "components/webdata_services/web_data_service_wrapper_factory.h"
#include "content/public/browser/browser_context.h"
#include "third_party/blink/public/common/features.h"

namespace payments {

namespace {

// Gets dependencies from the context and starts the asynchronous process to
// find browser bound keys and delete them.
static void RemoveInvalidBBKs(content::BrowserContext* context) {
  if (context->IsOffTheRecord()) {
    // There is no need to remove invalid BBKs for a derived OTR profile, since
    // it would have been done for the original profile.
    return;
  }
  scoped_refptr<PaymentManifestWebDataService> web_data_service =
      webdata_services::WebDataServiceWrapperFactory::
          GetPaymentManifestWebDataServiceForBrowserContext(
              context, ServiceAccessType::EXPLICIT_ACCESS);
  if (!web_data_service) {
    // If this context does not provide web data, then there is no BBK metadata
    // to remove.
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
      std::make_unique<PasskeyBrowserBinder>(bbk_key_store, web_data_service);
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

}  // namespace

// static
BrowserBoundKeyDeleterFactory* BrowserBoundKeyDeleterFactory::GetInstance() {
  static base::NoDestructor<BrowserBoundKeyDeleterFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
BrowserBoundKeyDeleterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (base::FeatureList::IsEnabled(
          blink::features::kSecurePaymentConfirmationBrowserBoundKeys)) {
    RemoveInvalidBBKs(context);
  }
  // Return an empty service object to avoid holding dependencies forever when
  // this service only runs a clean-up method on initialization.
  return std::make_unique<KeyedService>();
}

bool BrowserBoundKeyDeleterFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

BrowserBoundKeyDeleterFactory::BrowserBoundKeyDeleterFactory()
    : BrowserContextKeyedServiceFactory(
          "BrowserBoundKeyDeleter",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(webdata_services::WebDataServiceWrapperFactory::GetInstance());
}

BrowserBoundKeyDeleterFactory::~BrowserBoundKeyDeleterFactory() = default;

}  // namespace payments
