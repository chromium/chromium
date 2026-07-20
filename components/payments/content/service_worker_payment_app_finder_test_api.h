// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_SERVICE_WORKER_PAYMENT_APP_FINDER_TEST_API_H_
#define COMPONENTS_PAYMENTS_CONTENT_SERVICE_WORKER_PAYMENT_APP_FINDER_TEST_API_H_

#include <memory>
#include <string>

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "components/payments/content/payment_manifest_downloader.h"
#include "components/payments/content/service_worker_payment_app_finder.h"

namespace payments {

class ServiceWorkerPaymentAppFinderTestApi {
 public:
  explicit ServiceWorkerPaymentAppFinderTestApi(
      ServiceWorkerPaymentAppFinder* finder)
      : finder_(CHECK_DEREF(finder)) {}

  ServiceWorkerPaymentAppFinderTestApi(
      const ServiceWorkerPaymentAppFinderTestApi&) = delete;
  ServiceWorkerPaymentAppFinderTestApi& operator=(
      const ServiceWorkerPaymentAppFinderTestApi&) = delete;

  ~ServiceWorkerPaymentAppFinderTestApi() = default;

  // Should be called before every call to GetAllPaymentApps() (because the test
  // downloader is moved into the SelfDeletingServiceWorkerPaymentAppFinder).
  void SetDownloaderAndIgnorePortInOriginComparison(
      std::unique_ptr<PaymentManifestDownloader> downloader) {
    finder_->test_downloader_ = std::move(downloader);
  }

  // Ignore the given |method|, so that no installed or installable service
  // workers would ever be looked up in GetAllPaymentApps(). Calling this
  // multiple times will union the new payment methods with the existing set.
  void IgnorePaymentMethod(const std::string& method) {
    finder_->ignored_methods_.insert(method);
  }

 private:
  const raw_ref<ServiceWorkerPaymentAppFinder> finder_;
};

inline ServiceWorkerPaymentAppFinderTestApi test_api(
    ServiceWorkerPaymentAppFinder* finder) {
  return ServiceWorkerPaymentAppFinderTestApi(finder);
}

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SERVICE_WORKER_PAYMENT_APP_FINDER_TEST_API_H_
