// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android_payment_app_factory.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "base/supports_user_data.h"
#include "components/payments/content/android_app_communication.h"
#include "components/payments/content/android_payment_app.h"
#include "components/payments/content/content_payment_request_delegate.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/core/android_app_description.h"
#include "components/payments/core/android_app_description_tools.h"
#include "components/payments/core/method_strings.h"
#include "components/payments/core/native_error_strings.h"
#include "components/payments/core/payment_request_data_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/web_contents.h"

namespace payments {
namespace {

class AppFinder : public base::SupportsUserData::Data {
 public:
  static base::WeakPtr<AppFinder> CreateAndSetOwnedBy(
      base::SupportsUserData* owner) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK(owner);
    auto owned = std::make_unique<AppFinder>(owner);
    auto weak_ptr = owned->weak_ptr_factory_.GetWeakPtr();
    const void* key = owned.get();
    owner->SetUserData(key, std::move(owned));
    return weak_ptr;
  }

  explicit AppFinder(base::SupportsUserData* owner) : owner_(owner) {}
  ~AppFinder() override = default;

  AppFinder(const AppFinder& other) = delete;
  AppFinder& operator=(const AppFinder& other) = delete;

  void FindApps(base::WeakPtr<AndroidAppCommunication> communication,
                base::WeakPtr<PaymentAppFactory::Delegate> delegate) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK_EQ(nullptr, delegate_.get());
    DCHECK_NE(nullptr, delegate.get());
    DCHECK_EQ(0U, number_of_pending_is_ready_to_pay_queries_);
    DCHECK_EQ(nullptr, communication_.get());
    DCHECK_NE(nullptr, communication.get());
    DCHECK(delegate->GetSpec());
    DCHECK(delegate->GetSpec()->details().id.has_value());

    delegate_ = delegate;
    communication_ = communication;

    std::set<std::string> twa_payment_method_names = {
        methods::kGooglePlayBilling,
    };
    if (base::STLSetIntersection<std::set<std::string>>(
            delegate_->GetSpec()->payment_method_identifiers_set(),
            twa_payment_method_names)
            .empty()) {
      OnDoneCreatingPaymentApps();
      return;
    }

    delegate_->GetTwaPackageName(base::BindOnce(
        &AppFinder::OnGetTwaPackageName, weak_ptr_factory_.GetWeakPtr()));
  }

  void OnGetTwaPackageName(const std::string& twa_package_name) {
    if (twa_package_name.empty()) {
      OnDoneCreatingPaymentApps();
      return;
    }

    communication_->GetAppDescriptions(
        twa_package_name, base::BindOnce(&AppFinder::OnGetAppDescriptions,
                                         weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  // Check that our required dependencies are still valid, i.e. that the page
  // isn't currently being torn down.
  bool PageIsValid() {
    return communication_ && delegate_ && delegate_->GetSpec() &&
           delegate_->GetInitiatorRenderFrameHost();
  }

  void OnGetAppDescriptions(
      const std::optional<std::string>& error_message,
      std::vector<std::unique_ptr<AndroidAppDescription>> app_descriptions) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // The browser could be shutting down.
    if (!PageIsValid()) {
      return;
    }

    if (error_message.has_value()) {
      delegate_->OnPaymentAppCreationError(error_message.value());
      OnDoneCreatingPaymentApps();
      return;
    }

    std::vector<std::unique_ptr<AndroidAppDescription>> single_activity_apps;
    for (size_t i = 0; i < app_descriptions.size(); ++i) {
      auto app = std::move(app_descriptions[i]);
      if (app->service_names.size() > 1U) {
        delegate_->OnPaymentAppCreationError(errors::kMoreThanOneService);
        continue;
      }

      // Move each activity in the given |app| to its own AndroidAppDescription
      // in |single_activity_apps|, so the code can treat each PAY intent as its
      // own payment app. This allows Android apps to implement PAY intent in
      // multiple activities with different names and icons for different use
      // cases.
      SplitPotentiallyMultipleActivities(std::move(app), &single_activity_apps);
    }

    number_of_pending_is_ready_to_pay_queries_ = single_activity_apps.size();
    if (number_of_pending_is_ready_to_pay_queries_ == 0U) {
      OnDoneCreatingPaymentApps();
      return;
    }

    for (size_t i = 0; i < single_activity_apps.size(); ++i) {
      std::unique_ptr<AndroidAppDescription> single_activity_app =
          std::move(single_activity_apps[i]);

      const std::string& default_method =
          single_activity_app->activities.front()->default_payment_method;
      DCHECK_EQ(methods::kGooglePlayBilling, default_method);

      std::set<std::string> supported_payment_methods = {default_method};
      std::set<std::string> payment_method_names =
          base::STLSetIntersection<std::set<std::string>>(
              delegate_->GetSpec()->payment_method_identifiers_set(),
              supported_payment_methods);

      std::unique_ptr<std::map<std::string, std::set<std::string>>>
          stringified_method_data = data_util::FilterStringifiedMethodData(
              delegate_->GetSpec()->stringified_method_data(),
              supported_payment_methods);

      // TODO(crbug.com/40106647): Download the web app manifest for
      // |default_payment_method_name| to verify Android app signature.

      // Skip querying IS_READY_TO_PAY service when Chrome is off-the-record or
      // when the app does not implement the IS_READY_TO_PAY service.
      if (delegate_->IsOffTheRecord() ||
          single_activity_app->service_names.empty()) {
        OnIsReadyToPay(std::move(single_activity_app), payment_method_names,
                       std::move(stringified_method_data),
                       /*error_message=*/std::nullopt,
                       /*is_ready_to_pay=*/true);
        continue;
      }

      const std::string package = single_activity_app->package;
      const std::string service_name =
          single_activity_app->service_names.front();
      std::map<std::string, std::set<std::string>>
          stringified_method_data_copy = *stringified_method_data;
      communication_->IsReadyToPay(
          package, service_name, stringified_method_data_copy,
          delegate_->GetTopOrigin(), delegate_->GetFrameOrigin(),
          delegate_->GetSpec()->details().id.value(),
          base::BindOnce(&AppFinder::OnIsReadyToPay,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(single_activity_app), payment_method_names,
                         std::move(stringified_method_data)));
    }
  }

  void OnIsReadyToPay(
      std::unique_ptr<AndroidAppDescription> app_description,
      const std::set<std::string>& payment_method_names,
      std::unique_ptr<std::map<std::string, std::set<std::string>>>
          stringified_method_data,
      const std::optional<std::string>& error_message,
      bool is_ready_to_pay) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK_LT(0U, number_of_pending_is_ready_to_pay_queries_);

    // The browser could be shutting down.
    if (!PageIsValid()) {
      OnDoneCreatingPaymentApps();
      return;
    }

    if (error_message.has_value()) {
      delegate_->OnPaymentAppCreationError(error_message.value());
    } else if (is_ready_to_pay) {
      delegate_->OnPaymentAppCreated(std::make_unique<AndroidPaymentApp>(
          payment_method_names, std::move(stringified_method_data),
          delegate_->GetTopOrigin(), delegate_->GetFrameOrigin(),
          delegate_->GetSpec()->details().id.value(),
          std::move(app_description), communication_,
          delegate_->GetInitiatorRenderFrameHost()->GetGlobalId(),
          delegate_->GetChromeOSTWAInstanceId()));
    }

    if (--number_of_pending_is_ready_to_pay_queries_ == 0)
      OnDoneCreatingPaymentApps();
  }

  void OnDoneCreatingPaymentApps() {
    if (delegate_)
      delegate_->OnDoneCreatingPaymentApps();

    owner_->RemoveUserData(this);
  }

  raw_ptr<base::SupportsUserData> owner_;
  base::WeakPtr<PaymentAppFactory::Delegate> delegate_;
  size_t number_of_pending_is_ready_to_pay_queries_ = 0;
  base::WeakPtr<AndroidAppCommunication> communication_;

  base::WeakPtrFactory<AppFinder> weak_ptr_factory_{this};
};

}  // namespace

AndroidPaymentAppFactory::AndroidPaymentAppFactory(
    base::WeakPtr<AndroidAppCommunication> communication)
    : PaymentAppFactory(PaymentApp::Type::NATIVE_MOBILE_APP),
      communication_(communication) {
  DCHECK(communication_);
}

AndroidPaymentAppFactory::~AndroidPaymentAppFactory() = default;

void AndroidPaymentAppFactory::Create(base::WeakPtr<Delegate> delegate) {
  content::WebContents* web_contents = delegate->GetWebContents();
  if (web_contents) {
    auto app_finder = AppFinder::CreateAndSetOwnedBy(web_contents);
    app_finder->FindApps(communication_, delegate);
  }
}

}  // namespace payments
