// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_handler_navigation_throttle.h"

#include <cstddef>
#include <string>

#include "components/payments/content/payments_userdata_key.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_response_headers.h"

namespace payments {
constexpr char kApplicationJavascript[] = "application/javascript";
constexpr char kApplicationXml[] = "application/xml";
constexpr char kApplicationJson[] = "application/json";

PaymentHandlerNavigationThrottle::PaymentHandlerNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

PaymentHandlerNavigationThrottle::~PaymentHandlerNavigationThrottle() = default;

const char* PaymentHandlerNavigationThrottle::GetNameForLogging() {
  return "PaymentHandlerNavigationThrottle";
}

// static
void PaymentHandlerNavigationThrottle::MarkPaymentHandlerWebContents(
    content::WebContents* web_contents) {
  if (!web_contents)
    return;
  web_contents->SetUserData(kPaymentHandlerWebContentsUserDataKey,
                            std::make_unique<base::SupportsUserData::Data>());
}

// static
std::unique_ptr<PaymentHandlerNavigationThrottle>
PaymentHandlerNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  if (!handle || !handle->GetWebContents() ||
      !handle->GetWebContents()->GetUserData(
          kPaymentHandlerWebContentsUserDataKey)) {
    return nullptr;
  }
  return std::make_unique<PaymentHandlerNavigationThrottle>(handle);
}

content::NavigationThrottle::ThrottleCheckResult
PaymentHandlerNavigationThrottle::WillProcessResponse() {
  if (!navigation_handle())
    return PROCEED;
  const net::HttpResponseHeaders* response_headers =
      navigation_handle()->GetResponseHeaders();
  if (!response_headers)
    return PROCEED;

  std::string mime_type;
  response_headers->GetMimeType(&mime_type);
  // This allowlist is made to exclude edge-cases whose vulnerabilities could
  // be exploited (e.g., application/pdf, see crbug.com/1159267).
  if (base::StartsWith(mime_type, "text/",
                       base::CompareCase::INSENSITIVE_ASCII) ||
      base::StartsWith(mime_type, "image/",
                       base::CompareCase::INSENSITIVE_ASCII) ||
      base::StartsWith(mime_type, "video/",
                       base::CompareCase::INSENSITIVE_ASCII) ||
      mime_type == kApplicationJavascript || mime_type == kApplicationXml ||
      mime_type == kApplicationJson) {
    return PROCEED;
  }

  VLOG(0) << "Blocked the payment handler from navigating to a page "
          << navigation_handle()->GetURL().spec() << " of " << mime_type
          << " mime type.";
  return BLOCK_RESPONSE;
}
}  // namespace payments
