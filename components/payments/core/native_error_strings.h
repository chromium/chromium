// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_NATIVE_ERROR_STRINGS_H_
#define COMPONENTS_PAYMENTS_CORE_NATIVE_ERROR_STRINGS_H_

namespace payments {
namespace errors {

// These strings are referenced only from C++.

// Mojo call PaymentRequest::Init() must precede PaymentRequest::Abort().
extern const char kCannotAbortWithoutInit[];

// Mojo call PaymentRequest::Show() must precede PaymentRequest::Abort().
extern const char kCannotAbortWithoutShow[];

// Mojo call PaymentRequest::Init() must precede
// PaymentRequest::CanMakePayment().
extern const char kCannotCallCanMakePaymentWithoutInit[];

// Mojo call PaymentRequest::Init() must precede
// PaymentRequest::HasEnrolledInstrument().
extern const char kCannotCallHasEnrolledInstrumentWithoutInit[];

// Mojo call PaymentRequest::Init() must precede PaymentRequest::Complete().
extern const char kCannotCompleteWithoutInit[];

// Mojo call PaymentRequest::Show() must precede PaymentRequest::Complete().
extern const char kCannotCompleteWithoutShow[];

// Used when "serviceworker"."scope" string A in web app manifest B is not a
// valid URL and cannot be resolved as a relative URL either. This format should
// be used with base::ReplaceStringPlaceholders(fmt, {A, B}, nullptr).
extern const char kCannotResolveServiceWorkerScope[];

// Used when "serviceworker"."src" string A in web app manifest B is not a valid
// URL and cannot be resolved as a relative URL either. This format should be
// used with base::ReplaceStringPlaceholders(fmt, {A, B}, nullptr).
extern const char kCannotResolveServiceWorkerUrl[];

// Mojo call PaymentRequest::Init() must precede PaymentRequest::Retry().
extern const char kCannotRetryWithoutInit[];

// Mojo call PaymentRequest::Show() must precede PaymentRequest::Retry().
extern const char kCannotRetryWithoutShow[];

// Used when a payment method A has a cross-origin "Link:
// rel=payment-method-manifest" to the manifest B. This format should be used
// with base::ReplaceStringPlaceholders(fmt, {B, A}, nullptr).
extern const char kCrossOriginPaymentMethodManifestNotAllowed[];

// The URL A in web app manifest B's "serviceworker"."scope" must be of the same
// origin as the web app manifest itself. This format should be used with
// base::ReplaceStringPlaceholders(fmt, {A, B}, nullptr).
extern const char kCrossOriginServiceWorkerScopeNotAllowed[];

// The URL A in web app manifest B's "serviceworker"."src" must be of the same
// origin as the web app manifest itself. This format should be used with
// base::ReplaceStringPlaceholders(fmt, {A, B}, nullptr).
extern const char kCrossOriginServiceWorkerUrlNotAllowed[];

// The "default_applications" list on origin A is not allowed to contain a URL
// from origin B. This format should be used with
// base::ReplaceStringPlaceholders(format, {B, A}, nullptr).
extern const char kCrossOriginWebAppManifestNotAllowed[];

// The format for a detailed message about invalid SSL certificate. This format
// should be used with base::ReplaceChars() function, where "$" is the character
// to replace.
extern const char kDetailedInvalidSslCertificateMessageFormat[];

// Used when a HEAD request for URL A fails. This format should be used with
// base::ReplaceStringPlaceholders(fmt, {A}, nullptr).
extern const char kHttpHeadRequestFailed[];

// Used for HTTP redirects that are prohibited for payment method manifests.
// This format should be used with base::ReplaceStringPlaceholders(fmt,
// {http_code, http_code_phrase, original_url}, nullptr).
extern const char kHttpStatusCodeNotAllowed[];

// The "default_applications" list should contain exactly one URL for JIT
// install feature to work.
extern const char kInstallingMultipleDefaultAppsNotSupported[];

// Used to let the web developer know about an invalid payment manifest URL A.
// This format should be used with base::ReplaceStringPlaceholders(fmt, {A},
// nullptr).
extern const char kInvalidManifestUrl[];

// Web app manifest contains an empty or non-UTF8 service worker scope.
extern const char kInvalidServiceWorkerScope[];

// Web app manifest contains an empty or non-UTF8 service worker URL.
extern const char kInvalidServiceWorkerUrl[];

// Chrome refuses to provide any payment information to a website with an
// invalid SSL certificate.
extern const char kInvalidSslCertificate[];

// Used when the {"supportedMethods": "", data: {}} is required, but not
// provided.
extern const char kMethodDataRequired[];

// Used when non-empty "supportedMethods": "" is required, but not provided.
extern const char kMethodNameRequired[];

// The payment handler responded with an empty "details" field.
extern const char kMissingDetailsFromPaymentApp[];

// The payment handler responded with an empty "methodName" field.
extern const char kMissingMethodNameFromPaymentApp[];

// The format for the message about multiple payment methods that are not
// supported. This format should be used with base::ReplaceChars() function,
// where "$" is the character to replace.
extern const char kMultiplePaymentMethodsNotSupportedFormat[];

// Used when the payment method URL A does not have a "Link:
// rel=payment-method-manifest" HTTP header. This format should be used with
// base::ReplaceStringPlaceholders(fmt, {A}, nullptr).
extern const char kNoLinkRelPaymentMethodManifestHttpHeader[];

// Payment handler did not respond to the "paymentrequest" event.
extern const char kNoResponseToPaymentEvent[];

// Used when PaymentRequest::Init() has not been called, but should have been.
extern const char kNotInitialized[];

// Used when PaymentRequest::Show() has not been called, but should have been.
extern const char kNotShown[];

// The payment handler responded with an empty "payer name" field.
extern const char kPayerNameEmpty[];

// The payment handler responded with an empty "payer email" field.
extern const char kPayerEmailEmpty[];

// The payment handler responded with an empty "payer phone" field.
extern const char kPayerPhoneEmpty[];

// Used for errors about cross-site redirects from A to B. This format should be
// used with base::ReplaceStringPlaceholders(fmt, {A, B}, nullptr).
extern const char kPaymentManifestCrossSiteRedirectNotAllowed[];

// Used when downloading payment manifest URL A has failed. This format should
// be used with base::ReplaceStringPlaceholders(fmt, {A}, nullptr).
extern const char kPaymentManifestDownloadFailed[];

// Payment handler passed a non-object field "details" in response to the
// "paymentrequest" event.
extern const char kPaymentDetailsNotObject[];

// Payment handler passed a non-stringifiable field "details" in response to the
// "paymentrequest" event.
extern const char kPaymentDetailsStringifyError[];

// Used when the browser failed to fire the "paymentrequest" event without any
// actionable corrective action from the web developer.
extern const char kPaymentEventBrowserError[];

// Service worker timed out or stopped for some reason or was killed before the
// payment handler could respond to the "paymentrequest" event.
extern const char kPaymentEventServiceWorkerError[];

// Service worker timed out while responding to "paymentrequest" event.
extern const char kPaymentEventTimeout[];

// Payment handler navigated to a page with insecure context, invalid SSL, or
// malicious content.
extern const char kPaymentHandlerInsecureNavigation[];

// Payment handler encountered an internal error when handling the
// "paymentrequest" event.
extern const char kPaymentEventInternalError[];

// Payment handler rejected the promise passed into
// PaymentRequestEvent.respondWith() method.
extern const char kPaymentEventRejected[];

// Used when maximum number of redirects has been reached.
extern const char kReachedMaximumNumberOfRedirects[];

// The format for the message about a single payment method that is not
// supported. This format should be used with base::ReplaceChars() function,
// where "$" is the character to replace.
extern const char kSinglePaymentMethodNotSupportedFormat[];

// Used when non-empty "shippingOptionId": "" is required, but not provided.
extern const char kShippingOptionIdRequired[];

// The payment handler responded with an invalid shipping address.
extern const char kShippingAddressInvalid[];

// The payment handler responded with an empty "shipping option" field.
extern const char kShippingOptionEmpty[];

}  // namespace errors
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_NATIVE_ERROR_STRINGS_H_
