// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/native_error_strings.h"

namespace payments {
namespace errors {

const char kCannotAbortWithoutInit[] =
    "Attempted abort without initialization.";

const char kCannotAbortWithoutShow[] = "Attempted abort without show.";

const char kCannotCallCanMakePaymentWithoutInit[] =
    "Attempted canMakePayment without initialization.";

const char kCannotCallHasEnrolledInstrumentWithoutInit[] =
    "Attempted hasEnrolledInstrument without initialization.";

const char kCannotCompleteWithoutInit[] =
    "Attempted complete without initialization.";

const char kCannotCompleteWithoutShow[] = "Attempted complete without show.";

const char kCannotResolveServiceWorkerScope[] =
    "Cannot resolve the \"serviceworker\".\"scope\" $1 in web app manifest $2.";

const char kCannotResolveServiceWorkerUrl[] =
    "Cannot resolve the \"serviceworker\".\"src\" $1 in web app manifest $2.";

const char kCannotRetryWithoutInit[] =
    "Attempted retry without initialization.";

const char kCannotRetryWithoutShow[] = "Attempted retry without show.";

const char kCrossOriginPaymentMethodManifestNotAllowed[] =
    "Cross-origin payment method manifest \"$1\" not allowed for the payment "
    "method \"$2\".";

const char kCrossOriginServiceWorkerScopeNotAllowed[] =
    "Cross-origin \"serviceworker\".\"scope\" $1 not allowed in web app "
    "manifest $2.";

const char kCrossOriginServiceWorkerUrlNotAllowed[] =
    "Cross-origin \"serviceworker\".\"src\" $1 not allowed in web app manifest "
    "$2.";

const char kCrossOriginWebAppManifestNotAllowed[] =
    "Cross-origin default application $1 not allowed in payment method "
    "manifest $2.";

const char kDetailedInvalidSslCertificateMessageFormat[] =
    "SSL certificate is not valid. Security level: $.";

const char kHttpHeadRequestFailed[] =
    "Unable to make a HEAD request to \"$1\" for payment method manifest.";

const char kHttpStatusCodeNotAllowed[] =
    "HTTP status code $1 \"$2\" not allowed for payment method manifest "
    "\"$3\".";

const char kInstallingMultipleDefaultAppsNotSupported[] =
    "Installing multiple payment handlers from a single payment method "
    "manifest is not supported.";

const char kInvalidManifestUrl[] =
    "\"$1\" is not a valid payment manifest URL with HTTPS scheme (or HTTP "
    "scheme for localhost).";

const char kInvalidServiceWorkerScope[] =
    "\"serviceworker\".\"scope\" must be a non-empty UTF8 string.";

const char kInvalidServiceWorkerUrl[] =
    "\"serviceworker\".\"src\" must be a non-empty UTF8 string.";

const char kInvalidSslCertificate[] = "SSL certificate is not valid.";

const char kMethodDataRequired[] = "Method data required.";

const char kMethodNameRequired[] = "Method name required.";

const char kMissingDetailsFromPaymentApp[] =
    "Payment app returned invalid response. Missing field \"details\".";

const char kMissingMethodNameFromPaymentApp[] =
    "Payment app returned invalid response. Missing field \"methodName\".";

const char kMultiplePaymentMethodsNotSupportedFormat[] =
    "The payment methods $ are not supported.";

const char kNoLinkRelPaymentMethodManifestHttpHeader[] =
    "No \"Link: rel=payment-method-manifest\" HTTP header found at \"$1\".";

const char kNoResponseToPaymentEvent[] =
    "Payment handler did not respond to \"paymentrequest\" event.";

const char kNotInitialized[] = "Not initialized.";

const char kNotShown[] = "Not shown.";

const char kPayerEmailEmpty[] =
    "Payment app returned invalid response. Missing field \"payerEmail\".";

const char kPayerNameEmpty[] =
    "Payment app returned invalid response. Missing field \"payerName\".";

const char kPayerPhoneEmpty[] =
    "Payment app returned invalid response. Missing field \"payerPhone\".";

const char kPaymentManifestCrossSiteRedirectNotAllowed[] =
    "Cross-site redirect from \"$1\" to \"$2\" not allowed for payment "
    "manifests.";

const char kPaymentManifestDownloadFailed[] =
    "Unable to download payment manifest \"$1\".";

const char kPaymentDetailsNotObject[] =
    "Payment app returned invalid response. \"details\" field is not a "
    "dictionary.";

const char kPaymentDetailsStringifyError[] =
    "Payment app returned invalid response. Unable to JSON-serialize "
    "\"details\".";

const char kPaymentEventBrowserError[] =
    "Browser encountered an error when firing the \"paymentrequest\" event in "
    "the payment handler.";

const char kPaymentEventInternalError[] =
    "Payment handler encountered an internal error when handling the "
    "\"paymentrequest\" event.";

const char kPaymentEventRejected[] =
    "Payment handler rejected the promise passed into "
    "PaymentRequestEvent.respondWith(). This is how payment handlers close "
    "their own window programmatically.";

const char kReachedMaximumNumberOfRedirects[] =
    "Unable to download the payment manifest because reached the maximum "
    "number of redirects.";

const char kPaymentEventServiceWorkerError[] =
    "Payment handler failed to provide a response because either the "
    "\"paymentrequest\" event took too long or the service worker stopped for "
    "some reason or was killed before the request finished.";

const char kPaymentEventTimeout[] =
    "The \"paymentrequest\" event timed out after 5 minutes.";

const char kPaymentHandlerInsecureNavigation[] =
    "The payment handler navigated to a page with insecure context, invalid "
    "certificate state, or malicious content.";

const char kSinglePaymentMethodNotSupportedFormat[] =
    "The payment method $ is not supported.";

const char kShippingOptionIdRequired[] = "Shipping option identifier required.";

const char kShippingAddressInvalid[] =
    "Payment app returned invalid shipping address in response.";

const char kShippingOptionEmpty[] =
    "Payment app returned invalid response. Missing field \"shipping option\".";

}  // namespace errors
}  // namespace payments
