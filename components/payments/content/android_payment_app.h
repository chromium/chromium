// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_APP_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_APP_H_

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "components/payments/content/android_app_communication.h"
#include "components/payments/content/payment_app.h"
#include "components/payments/core/android_app_description.h"
#include "content/public/browser/global_routing_id.h"
#include "url/gurl.h"

namespace payments {

// A cross-platform way to invoke an Android payment app.
class AndroidPaymentApp : public PaymentApp {
 public:
  // Creates an instance of AndroidPaymentApp.
  //
  // The |payment_method_names| is the set of payment method identifiers
  // supported by this app, e.g., ["https://example1.com",
  // "https://example2.com"]. This set should not be empty.
  //
  // The |stringified_method_data| is a mapping from payment method identifiers
  // that this app can handle to the method-specific data provided by the
  // merchant. The set of keys should match exactly the |payment_method_names|.
  // It is the responsibility of the creator of AndroidPaymentApp to filter out
  // the data from merchant that is not in |payment_method_names|.
  AndroidPaymentApp(
      const std::set<std::string>& payment_method_names,
      std::unique_ptr<std::map<std::string, std::set<std::string>>>
          stringified_method_data,
      const GURL& top_level_origin,
      const GURL& payment_request_origin,
      const std::string& payment_request_id,
      std::unique_ptr<AndroidAppDescription> description,
      base::WeakPtr<AndroidAppCommunication> communication,
      content::GlobalRenderFrameHostId frame_routing_id,
      const std::optional<base::UnguessableToken>& twa_instance_identifier);
  ~AndroidPaymentApp() override;

  AndroidPaymentApp(const AndroidPaymentApp& other) = delete;
  AndroidPaymentApp& operator=(const AndroidPaymentApp& other) = delete;

  // PaymentApp implementation.
  void InvokePaymentApp(base::WeakPtr<Delegate> delegate) override;
  bool IsCompleteForPayment() const override;
  bool CanPreselect() const override;
  std::u16string GetMissingInfoLabel() const override;
  bool HasEnrolledInstrument() const override;
  void RecordUse() override;
  bool NeedsInstallation() const override;
  std::string GetId() const override;
  std::u16string GetLabel() const override;
  std::u16string GetSublabel() const override;
  const SkBitmap* icon_bitmap() const override;
  bool IsValidForModifier(const std::string& method) const override;
  base::WeakPtr<PaymentApp> AsWeakPtr() override;
  bool HandlesShippingAddress() const override;
  bool HandlesPayerName() const override;
  bool HandlesPayerEmail() const override;
  bool HandlesPayerPhone() const override;
  bool IsWaitingForPaymentDetailsUpdate() const override;
  void UpdateWith(
      mojom::PaymentRequestDetailsUpdatePtr details_update) override;
  void OnPaymentDetailsNotUpdated() override;
  void AbortPaymentApp(base::OnceCallback<void(bool)> abort_callback) override;
  bool IsPreferred() const override;

 private:
  void OnPaymentAppResponse(base::WeakPtr<Delegate> delegate,
                            const std::optional<std::string>& error_message,
                            bool is_activity_result_ok,
                            const std::string& payment_method_identifier,
                            const std::string& stringified_details);

  const std::unique_ptr<std::map<std::string, std::set<std::string>>>
      stringified_method_data_;
  const GURL top_level_origin_;
  const GURL payment_request_origin_;
  const std::string payment_request_id_;
  const std::unique_ptr<AndroidAppDescription> description_;
  base::WeakPtr<AndroidAppCommunication> communication_;
  content::GlobalRenderFrameHostId frame_routing_id_;

  // Token used to uniquely identify a particular payment app instance between
  // Android and Chrome.
  base::UnguessableToken payment_app_token_;
  // True when InvokePaymentApp() has been called but no response has been
  // received yet.
  bool payment_app_open_;
  // Token used to uniquely identify a particular TWA instance that invoked
  // the payment request. This is used to map the TWA instance in
  // Lacros (the Chrome OS browser) and browser window that hosts the TWA
  // instance in Ash (the Chrome OS system UI).
  std::optional<base::UnguessableToken> twa_instance_identifier_;

  base::WeakPtrFactory<AndroidPaymentApp> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_APP_H_
