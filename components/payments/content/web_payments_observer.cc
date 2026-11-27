// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/web_payments_observer.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/supports_user_data.h"
#include "components/payments/core/features.h"
#include "components/payments/core/merchant_page_classifier.h"
#include "components/payments/core/web_payments_telemetry.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "url/gurl.h"

namespace payments {

namespace {

const char kMerchantPageClassifierUserDataKey[] =
    "payments_merchant_page_classifier";

class MerchantPageClassifierData : public base::SupportsUserData::Data {
 public:
  MerchantPageClassifierData() = default;
  MerchantPageClassifierData(const MerchantPageClassifierData&) = delete;
  MerchantPageClassifierData& operator=(const MerchantPageClassifierData&) =
      delete;
  ~MerchantPageClassifierData() override = default;

  const MerchantPageClassifier& classifier() const { return classifier_; }

 private:
  MerchantPageClassifier classifier_;
};

// Caches one compiled MerchantPageClassifier per BrowserContext so tabs in the
// same profile share the compiled regexes.
const MerchantPageClassifier& GetOrCreateMerchantPageClassifier(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(browser_context);

  auto* data = static_cast<MerchantPageClassifierData*>(
      browser_context->GetUserData(kMerchantPageClassifierUserDataKey));
  if (!data) {
    auto new_data = std::make_unique<MerchantPageClassifierData>();
    data = new_data.get();
    browser_context->SetUserData(kMerchantPageClassifierUserDataKey,
                                 std::move(new_data));
  }
  return data->classifier();
}

}  // namespace

WebPaymentsObserver::WebPaymentsObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

WebPaymentsObserver::~WebPaymentsObserver() = default;

void WebPaymentsObserver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle) {
    return;
  }

  if (base::FeatureList::IsEnabled(features::kThreeDSecureTelemetry)) {
    RecordThreeDSecureTelemetry(navigation_handle);
  }
}

void WebPaymentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  if (navigation_handle->IsSameDocument() &&
      navigation_handle->GetURL() ==
          navigation_handle->GetPreviousPrimaryMainFrameURL()) {
    return;
  }

  should_classify_current_navigation_ = false;
  classified_checkout_ = false;
  classified_order_confirmation_ = false;

  // TODO(crbug.com/586559294): Also skip off-the-record profiles, error pages,
  // history navigations, reloads, and opaque origins.
  if (!navigation_handle->GetURL().SchemeIsHTTPOrHTTPS()) {
    return;
  }

  if (!base::FeatureList::IsEnabled(features::kCheckoutTelemetry)) {
    return;
  }

  // TODO(crbug.com/586561783): Gate classification on the merchant allowlist.
  // Keep this after `IsEnabled(kCheckoutTelemetry)` so the Control group does
  // not load the allowlist into memory.
  should_classify_current_navigation_ = true;
}

void WebPaymentsObserver::TitleWasSet(content::NavigationEntry* entry) {
  // A page is only classified when its title is set, so a page whose title
  // never changes after the navigation commits is not classified.
  // TODO(crbug.com/574458447): Record how often that happens.
  if (!should_classify_current_navigation_ ||
      !base::FeatureList::IsEnabled(features::kCheckoutTelemetry) ||
      entry != web_contents()->GetController().GetLastCommittedEntry()) {
    return;
  }

  ClassifyMerchantPage(entry->GetURL(), entry->GetTitle());
}

void WebPaymentsObserver::RecordThreeDSecureTelemetry(
    content::NavigationHandle* navigation_handle) {
  // Filter for only HTTP POST requests from form submissions.
  if (!navigation_handle->IsPost() || !navigation_handle->IsFormSubmission()) {
    return;
  }

  scoped_refptr<const network::ResourceRequestBody> post_data =
      navigation_handle->GetPostData();
  if (!post_data || !post_data->elements()) {
    return;
  }

  for (const network::DataElement& element : *post_data->elements()) {
    if (const auto* bytes_element =
            element.TryAs<network::DataElementBytes>()) {
      RecordThreeDSecureTelemetryFromFormData(
          bytes_element->AsStringView(),
          navigation_handle->GetNextPageUkmSourceId());
    }
  }
}

void WebPaymentsObserver::ClassifyMerchantPage(const GURL& url,
                                               std::u16string_view page_title) {
  if (classified_checkout_ && classified_order_confirmation_) {
    return;
  }

  const MerchantClassificationResult result =
      GetOrCreateMerchantPageClassifier(web_contents()->GetBrowserContext())
          .Classify(url, page_title);
  // TODO(crbug.com/574458447): Read `classified_checkout_` and
  // `classified_order_confirmation_` here to record each UMA histogram at most
  // once per navigation (for now they are only read by `*_for_testing()`).
  if (result.page_type == MerchantPageType::kCheckoutPage) {
    classified_checkout_ = true;
  } else if (result.page_type == MerchantPageType::kOrderConfirmationPage) {
    classified_order_confirmation_ = true;
  }
}

}  // namespace payments
