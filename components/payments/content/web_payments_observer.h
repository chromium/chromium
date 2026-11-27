// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_WEB_PAYMENTS_OBSERVER_H_
#define COMPONENTS_PAYMENTS_CONTENT_WEB_PAYMENTS_OBSERVER_H_

#include <string_view>

#include "content/public/browser/web_contents_observer.h"

class GURL;

namespace content {
class NavigationEntry;
class NavigationHandle;
class WebContents;
}  // namespace content

namespace payments {

// WebPaymentsObserver observes changes in the web contents, to measure web
// payments flows.
//
// In order to help the payments industry (via the W3C Web Payments working
// Group) understand and improve the user experience of payment flows in
// Chromium, this class records basic anonymized metrics (for clients which are
// opted into metrics collection). Only technical information for such payment
// flows is ever recorded, never any personal information or transaction
// details.
//
// Currently we record such metrics for:
//
// * 3D-Secure payment challenges (see RecordThreeDSecureTelemetry).
// * Merchant checkout and order confirmation pages (see ClassifyMerchantPage).
class WebPaymentsObserver : public content::WebContentsObserver {
 public:
  explicit WebPaymentsObserver(content::WebContents* web_contents);
  WebPaymentsObserver(const WebPaymentsObserver&) = delete;
  WebPaymentsObserver& operator=(const WebPaymentsObserver&) = delete;
  ~WebPaymentsObserver() override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void TitleWasSet(content::NavigationEntry* entry) override;

  // TODO(crbug.com/574458447): Remove these getters once checkout telemetry
  // UMA histograms are recorded.
  bool classified_checkout_for_testing() const { return classified_checkout_; }
  bool classified_order_confirmation_for_testing() const {
    return classified_order_confirmation_;
  }

 private:
  // Records metrics for 3DS (3D-Secure) Challenge Requests and Challenge
  // Responses. The metrics are used to help the payments industry understand
  // and improve the user experience of 3D-Secure payments challenges in
  // Chromium.
  //
  // It does this by filtering for HTTP POST requests from form
  // submissions that contain `creq` (for Challenge Requests) or `cres` (for
  // Challenge Responses) in the form data, as per the 3D-Secure specification.
  // The total count of these requests and responses are recorded.
  //
  // In addition, it will parse the `cres` value and record metrics on the
  // outcome of the response from the `transStatus` field.
  void RecordThreeDSecureTelemetry(
      content::NavigationHandle* navigation_handle);

  // Checks whether `url` and `page_title` match a checkout or order
  // confirmation page and remembers the result for the current navigation.
  void ClassifyMerchantPage(const GURL& url, std::u16string_view page_title);

  bool should_classify_current_navigation_ = false;
  bool classified_checkout_ = false;
  bool classified_order_confirmation_ = false;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_WEB_PAYMENTS_OBSERVER_H_
