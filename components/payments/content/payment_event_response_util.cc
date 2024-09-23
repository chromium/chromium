// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_event_response_util.h"

#include <string_view>

#include "components/payments/core/error_strings.h"
#include "components/payments/core/native_error_strings.h"

namespace payments {

std::string_view ConvertCanMakePaymentEventResponseTypeToErrorString(
    mojom::CanMakePaymentEventResponseType response_type) {
  switch (response_type) {
    case mojom::CanMakePaymentEventResponseType::BOOLEAN_CONVERSION_ERROR:
      return errors::kCanMakePaymentEventBooleanConversionError;
    case mojom::CanMakePaymentEventResponseType::BROWSER_ERROR:
      return errors::kCanMakePaymentEventBrowserError;
    case mojom::CanMakePaymentEventResponseType::INTERNAL_ERROR:
      return errors::kCanMakePaymentEventInternalError;
    case mojom::CanMakePaymentEventResponseType::NO_EXPLICITLY_VERIFIED_METHODS:
      return errors::kCanMakePaymentEventNoExplicitlyVerifiedMethods;
    case mojom::CanMakePaymentEventResponseType::NO_RESPONSE:
      return errors::kCanMakePaymentEventNoResponse;
    case mojom::CanMakePaymentEventResponseType::NOT_INSTALLED:
      return errors::kCanMakePaymentEventNotInstalled;
    case mojom::CanMakePaymentEventResponseType::NO_URL_BASED_PAYMENT_METHODS:
      return errors::kCanMakePaymentEventNoUrlBasedPaymentMethods;
    case mojom::CanMakePaymentEventResponseType::REJECT:
      return errors::kCanMakePaymentEventRejected;
    case mojom::CanMakePaymentEventResponseType::TIMEOUT:
      return errors::kCanMakePaymentEventTimeout;
    case mojom::CanMakePaymentEventResponseType::INCOGNITO:
    // Intentionally fallthrough.
    case mojom::CanMakePaymentEventResponseType::SUCCESS:
      return "";
  }
}

std::string_view ConvertPaymentEventResponseTypeToErrorString(
    mojom::PaymentEventResponseType response_type) {
  switch (response_type) {
    case mojom::PaymentEventResponseType::PAYMENT_EVENT_SUCCESS:
      return "";
    case mojom::PaymentEventResponseType::PAYMENT_EVENT_REJECT:
      return errors::kPaymentEventRejected;
    case mojom::PaymentEventResponseType::PAYMENT_EVENT_SERVICE_WORKER_ERROR:
      return errors::kPaymentEventServiceWorkerError;
    case mojom::PaymentEventResponseType::PAYMENT_HANDLER_WINDOW_CLOSING:
      return errors::kUserCancelled;
    case mojom::PaymentEventResponseType::PAYMENT_EVENT_INTERNAL_ERROR:
      return errors::kPaymentEventInternalError;
    case mojom::PaymentEventResponseType::PAYMENT_EVENT_NO_RESPONSE:
      return errors::kNoResponseToPaymentEvent;
    case mojom::PaymentEventResponseType::PAYMENT_DETAILS_STRINGIFY_ERROR:
      return errors::kPaymentDetailsStringifyError;
    case mojom::PaymentEventResponseType::PAYMENT_METHOD_NAME_EMPTY:
      return errors::kMissingMethodNameFromPaymentApp;
    case mojom::PaymentEventResponseType::PAYMENT_DETAILS_ABSENT:
      return errors::kMissingDetailsFromPaymentApp;
    case mojom::PaymentEventResponseType::PAYMENT_DETAILS_NOT_OBJECT:
      return errors::kPaymentDetailsNotObject;
    case mojom::PaymentEventResponseType::PAYMENT_EVENT_BROWSER_ERROR:
      return errors::kPaymentEventBrowserError;
    case mojom::PaymentEventResponseType::PAYMENT_EVENT_TIMEOUT:
      return errors::kPaymentEventTimeout;
    case mojom::PaymentEventResponseType::PAYMENT_HANDLER_INSECURE_NAVIGATION:
      return errors::kPaymentHandlerInsecureNavigation;
    case mojom::PaymentEventResponseType::PAYMENT_HANDLER_INSTALL_FAILED:
      return errors::kPaymentHandlerInstallFailed;
    case mojom::PaymentEventResponseType::PAYMENT_HANDLER_ACTIVITY_DIED:
      return errors::kPaymentHandlerActivityDied;
    case mojom::PaymentEventResponseType::
        PAYMENT_HANDLER_FAIL_TO_LOAD_MAIN_FRAME:
      return errors::kPaymentHandlerFailToLoadMainFrame;
    case mojom::PaymentEventResponseType::PAYER_NAME_EMPTY:
      return errors::kPayerNameEmpty;
    case mojom::PaymentEventResponseType::PAYER_EMAIL_EMPTY:
      return errors::kPayerEmailEmpty;
    case mojom::PaymentEventResponseType::PAYER_PHONE_EMPTY:
      return errors::kPayerPhoneEmpty;
    case mojom::PaymentEventResponseType::SHIPPING_ADDRESS_INVALID:
      return errors::kShippingAddressInvalid;
    case mojom::PaymentEventResponseType::SHIPPING_OPTION_EMPTY:
      return errors::kShippingOptionEmpty;
  }
}

}  // namespace payments
