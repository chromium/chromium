// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_app_service.h"

#include <utility>

#include "base/feature_list.h"
#include "components/payments/content/android_app_communication.h"
#include "components/payments/content/android_payment_app_factory.h"
#include "components/payments/content/payment_app.h"
#include "components/payments/content/secure_payment_confirmation_app_factory.h"
#include "components/payments/content/service_worker_payment_app_factory.h"
#include "components/payments/core/features.h"
#include "components/payments/core/payments_experimental_features.h"
#include "content/public/common/content_features.h"

namespace payments {

PaymentAppService::PaymentAppService(content::BrowserContext* context) {
  if (base::FeatureList::IsEnabled(::features::kServiceWorkerPaymentApps)) {
    factories_.push_back(std::make_unique<ServiceWorkerPaymentAppFactory>());
  }

  // TODO(crbug.com/40106647): Review the feature flag name when
  // AndroidPaymentAppFactory works on Android OS with generic 3rd party payment
  // apps. (Currently it works only on Chrome OS with app store billing payment
  // methods.)
  if (PaymentsExperimentalFeatures::IsEnabled(features::kAppStoreBilling)) {
    factories_.push_back(std::make_unique<AndroidPaymentAppFactory>(
        AndroidAppCommunication::GetForBrowserContext(context)));
  }

  // SecurePaymentConfirmation is enabled if both the feature flag and the Blink
  // runtime feature "SecurePaymentConfirmation" are enabled.
  if (base::FeatureList::IsEnabled(::features::kSecurePaymentConfirmation)) {
    factories_.push_back(
        std::make_unique<SecurePaymentConfirmationAppFactory>());
  }
}

PaymentAppService::~PaymentAppService() = default;

size_t PaymentAppService::GetNumberOfFactories() const {
  return factories_.size();
}

void PaymentAppService::Create(
    base::WeakPtr<PaymentAppFactory::Delegate> delegate) {
  for (const auto& factory : factories_) {
    factory->Create(delegate);
  }
}

void PaymentAppService::AddFactoryForTesting(
    std::unique_ptr<PaymentAppFactory> factory) {
  factories_.push_back(std::move(factory));
}

}  // namespace payments
