// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENT_APP_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENT_APP_H_

#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/payments/core/payer_data.h"
#include "ui/gfx/image/image_skia.h"

namespace payments {

// Base class which represents a payment app in Payment Request.
class PaymentApp {
 public:
  // The type of this app instance.
  enum class Type { AUTOFILL, NATIVE_MOBILE_APP, SERVICE_WORKER_APP };

  class Delegate {
   public:
    virtual ~Delegate() {}

    // Should be called with method name (e.g., "https://google.com/pay") and
    // json-serialized stringified details.
    virtual void OnInstrumentDetailsReady(
        const std::string& method_name,
        const std::string& stringified_details,
        const PayerData& payer_data) = 0;

    // Should be called with a developer-facing error message to be used when
    // rejecting PaymentRequest.show().
    virtual void OnInstrumentDetailsError(const std::string& error_message) = 0;
  };

  virtual ~PaymentApp();

  // Will call into the |delegate| (can't be null) on success or error.
  virtual void InvokePaymentApp(Delegate* delegate) = 0;
  // Called when the payment app window has closed.
  virtual void OnPaymentAppWindowClosed() {}
  // Returns whether the app is complete to be used for payment without further
  // editing.
  virtual bool IsCompleteForPayment() const = 0;
  // Returns the calculated completeness score. Used to sort the list of
  // available apps.
  virtual uint32_t GetCompletenessScore() const = 0;
  // Returns whether the app can be preselected in the payment sheet. If none of
  // the apps can be preselected, the user must explicitly select an app from a
  // list.
  virtual bool CanPreselect() const = 0;
  // Returns whether the app is exactly matching all filters provided by the
  // merchant. For example, this can return "false" for unknown autofill card
  // types, if the merchant requested only debit cards from "basic-card".
  virtual bool IsExactlyMatchingMerchantRequest() const = 0;
  // Returns a message to indicate to the user what's missing for the app to be
  // complete for payment.
  virtual base::string16 GetMissingInfoLabel() const = 0;
  // Returns whether the app is valid for the purposes of responding to
  // canMakePayment.
  // TODO(crbug.com/915907): rename to IsValidForHasEnrolledInstrument.
  virtual bool IsValidForCanMakePayment() const = 0;
  // Records the use of this payment app.
  virtual void RecordUse() = 0;
  // Return the sub/label of payment app, to be displayed to the user.
  virtual base::string16 GetLabel() const = 0;
  virtual base::string16 GetSublabel() const = 0;
  virtual gfx::ImageSkia icon_image_skia() const;

  // Returns true if this payment app can be used to fulfill a request
  // specifying |method| as supported method of payment. The parsed basic-card
  // specific data (supported_networks, supported_types, etc) is relevant only
  // for the AutofillPaymentApp, which runs inside of the browser process and
  // thus should not be parsing untrusted JSON strings from the renderer.
  virtual bool IsValidForModifier(
      const std::string& method,
      bool supported_networks_specified,
      const std::set<std::string>& supported_networks,
      bool supported_types_specified,
      const std::set<autofill::CreditCard::CardType>& supported_types)
      const = 0;

  // Sets |is_valid| to true if this payment app can handle payments for the
  // given |payment_method_identifier|. The |is_valid| is an out-param instead
  // of the return value to enable binding this method with a base::WeakPtr,
  // which prohibits non-void methods.
  virtual void IsValidForPaymentMethodIdentifier(
      const std::string& payment_method_identifier,
      bool* is_valid) const = 0;

  // Returns a WeakPtr to this payment app.
  virtual base::WeakPtr<PaymentApp> AsWeakPtr() = 0;

  // Returns true if this payment app can collect and return the required
  // information. This is used to show/hide shipping/contact sections in payment
  // sheet view depending on the selected app.
  virtual bool HandlesShippingAddress() const = 0;
  virtual bool HandlesPayerName() const = 0;
  virtual bool HandlesPayerEmail() const = 0;
  virtual bool HandlesPayerPhone() const = 0;

  // Sorts the apps using the overloaded < operator.
  static void SortApps(std::vector<std::unique_ptr<PaymentApp>>* apps);
  static void SortApps(std::vector<PaymentApp*>* apps);

  int icon_resource_id() const { return icon_resource_id_; }
  Type type() const { return type_; }

 protected:
  PaymentApp(int icon_resource_id, Type type);

 private:
  bool operator<(const PaymentApp& other) const;
  int icon_resource_id_;
  Type type_;

  DISALLOW_COPY_AND_ASSIGN(PaymentApp);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENT_APP_H_
