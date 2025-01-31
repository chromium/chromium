// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/service_worker_payment_app_factory.h"

#include <map>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/developer_console_logger.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/service_worker_payment_app.h"
#include "components/payments/content/service_worker_payment_app_finder.h"
#include "components/payments/core/error_message_util.h"
#include "components/payments/core/features.h"
#include "components/payments/core/method_strings.h"
#include "content/public/browser/stored_payment_app.h"
#include "content/public/browser/supported_delegations.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"

namespace payments {

class ServiceWorkerPaymentAppCreator {
 public:
  explicit ServiceWorkerPaymentAppCreator(
      base::WeakPtr<PaymentAppFactory::Delegate> delegate)
      : delegate_(delegate), log_(delegate->GetWebContents()) {}

  ServiceWorkerPaymentAppCreator(const ServiceWorkerPaymentAppCreator&) =
      delete;
  ServiceWorkerPaymentAppCreator& operator=(
      const ServiceWorkerPaymentAppCreator&) = delete;

  ~ServiceWorkerPaymentAppCreator() = default;

  void CreatePaymentApps(
      content::InstalledPaymentAppsFinder::PaymentApps apps,
      ServiceWorkerPaymentAppFinder::InstallablePaymentApps installable_apps,
      const std::string& error_message) {
    if (!delegate_ || !delegate_->GetSpec() || !delegate_->GetWebContents() ||
        !delegate_->GetInitiatorRenderFrameHost()) {
      FinishAndCleanup();
      return;
    }

    if (!error_message.empty())
      delegate_->OnPaymentAppCreationError(error_message);

    base::RepeatingClosure show_processing_spinner = base::BindRepeating(
        &PaymentAppFactory::Delegate::ShowProcessingSpinner, delegate_);
    std::vector<std::string> skipped_app_names;
    for (auto& installed_app : apps) {
      std::vector<std::string> enabled_methods =
          installed_app.second->enabled_methods;
      bool has_app_store_billing_method =
          base::Contains(enabled_methods, methods::kGooglePlayBilling);
      if (ShouldSkipAppForPartialDelegation(
              installed_app.second->supported_delegations, delegate_,
              has_app_store_billing_method)) {
        skipped_app_names.emplace_back(installed_app.second->name);
        continue;
      }
      auto app = std::make_unique<ServiceWorkerPaymentApp>(
          delegate_->GetWebContents(), delegate_->GetTopOrigin(),
          delegate_->GetFrameOrigin(), delegate_->GetSpec(),
          std::move(installed_app.second), delegate_->IsOffTheRecord(),
          show_processing_spinner);
      app->ValidateCanMakePayment(base::BindOnce(
          &ServiceWorkerPaymentAppCreator::OnSWPaymentAppValidated,
          weak_ptr_factory_.GetWeakPtr()));
      PaymentApp* raw_payment_app_pointer = app.get();
      available_apps_[raw_payment_app_pointer] = std::move(app);
      number_of_pending_sw_payment_apps_++;
    }

    for (auto& installable_app : installable_apps) {
      bool is_app_store_billing_method =
          installable_app.first.spec() == methods::kGooglePlayBilling;
      if (ShouldSkipAppForPartialDelegation(
              installable_app.second->supported_delegations, delegate_,
              is_app_store_billing_method)) {
        skipped_app_names.emplace_back(installable_app.second->name);
        continue;
      }
      auto app = std::make_unique<ServiceWorkerPaymentApp>(
          delegate_->GetWebContents(), delegate_->GetTopOrigin(),
          delegate_->GetFrameOrigin(), delegate_->GetSpec(),
          std::move(installable_app.second), installable_app.first.spec(),
          delegate_->IsOffTheRecord(), show_processing_spinner);
      app->ValidateCanMakePayment(base::BindOnce(
          &ServiceWorkerPaymentAppCreator::OnSWPaymentAppValidated,
          weak_ptr_factory_.GetWeakPtr()));
      PaymentApp* raw_payment_app_pointer = app.get();
      available_apps_[raw_payment_app_pointer] = std::move(app);
      number_of_pending_sw_payment_apps_++;
    }

    if (!skipped_app_names.empty()) {
      std::string warning_message =
          GetAppsSkippedForPartialDelegationErrorMessage(skipped_app_names);
      log_.Warn(warning_message);
    }

    if (number_of_pending_sw_payment_apps_ == 0U) {
      if (error_message.empty() && !skipped_app_names.empty()) {
        std::string new_error_message =
            GetAppsSkippedForPartialDelegationErrorMessage(skipped_app_names);
        delegate_->OnPaymentAppCreationError(new_error_message);
      }
      FinishAndCleanup();
    }
  }

  bool ShouldSkipAppForPartialDelegation(
      const content::SupportedDelegations& supported_delegations,
      const base::WeakPtr<PaymentAppFactory::Delegate>& delegate,
      bool has_app_store_billing_method) const {
    DCHECK(delegate);
    DCHECK(delegate->GetSpec());
    return (base::FeatureList::IsEnabled(features::kEnforceFullDelegation) ||
            has_app_store_billing_method) &&
           !supported_delegations.ProvidesAll(
               delegate->GetSpec()->payment_options());
  }

  base::WeakPtr<ServiceWorkerPaymentAppCreator> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void OnSWPaymentAppValidated(base::WeakPtr<ServiceWorkerPaymentApp> app) {
    if (!app || !delegate_) {
      FinishAndCleanup();
      return;
    }

    auto iterator = available_apps_.find(app.get());
    if (iterator != available_apps_.end()) {
      delegate_->OnPaymentAppCreated(std::move(iterator->second));
      available_apps_.erase(iterator);
    }

    if (--number_of_pending_sw_payment_apps_ == 0)
      FinishAndCleanup();
  }

  void FinishAndCleanup() {
    if (delegate_)
      delegate_->OnDoneCreatingPaymentApps();
  }

  base::WeakPtr<PaymentAppFactory::Delegate> delegate_;
  std::map<PaymentApp*, std::unique_ptr<PaymentApp>> available_apps_;
  DeveloperConsoleLogger log_;
  int number_of_pending_sw_payment_apps_ = 0;

  base::WeakPtrFactory<ServiceWorkerPaymentAppCreator> weak_ptr_factory_{this};
};

ServiceWorkerPaymentAppFactory::ServiceWorkerPaymentAppFactory()
    : PaymentAppFactory(PaymentApp::Type::SERVICE_WORKER_APP) {}

ServiceWorkerPaymentAppFactory::~ServiceWorkerPaymentAppFactory() = default;

void ServiceWorkerPaymentAppFactory::Create(base::WeakPtr<Delegate> delegate) {
  auto* rfh = delegate->GetInitiatorRenderFrameHost();
  // Exit if frame or page is being unloaded or payments are otherwise
  // disallowed.
  if (!rfh || !rfh->IsActive() || !delegate->GetWebContents() ||
      !rfh->IsFeatureEnabled(blink::mojom::PermissionsPolicyFeature::kPayment))
    return;

  creator_ = std::make_unique<ServiceWorkerPaymentAppCreator>(delegate);

  ServiceWorkerPaymentAppFinder::GetOrCreateForCurrentDocument(rfh)
      ->GetAllPaymentApps(
          delegate->GetFrameSecurityOrigin(),
          delegate->GetPaymentManifestWebDataService(),
          mojo::Clone(delegate->GetMethodData()), delegate->GetCSPChecker(),
          base::BindOnce(&ServiceWorkerPaymentAppCreator::CreatePaymentApps,
                         creator_->GetWeakPtr()),
          base::BindOnce([]() {
            // Nothing needs to be done after writing cache. This callback is
            // used only in tests.
          }));
}

}  // namespace payments
