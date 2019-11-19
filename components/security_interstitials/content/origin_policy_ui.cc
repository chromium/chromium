// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/origin_policy_ui.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/optional.h"
#include "components/security_interstitials/content/origin_policy_interstitial_page.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "content/public/browser/navigation_handle.h"
#include "services/network/public/cpp/origin_policy.h"
#include "url/gurl.h"

namespace security_interstitials {

namespace {

std::unique_ptr<SecurityInterstitialPage> GetErrorPageImpl(
    network::OriginPolicyState error_reason,
    content::WebContents* web_contents,
    const GURL& url) {
  MetricsHelper::ReportDetails report_details;
  report_details.metric_prefix = "origin_policy";
  std::unique_ptr<SecurityInterstitialControllerClient> controller =
      std::make_unique<SecurityInterstitialControllerClient>(
          web_contents,
          std::make_unique<MetricsHelper>(url, report_details, nullptr),
          nullptr, /* pref service: can be null */
          "", GURL());
  return std::make_unique<security_interstitials::OriginPolicyInterstitialPage>(
      web_contents, url, std::move(controller), error_reason);
}

}  // namespace

base::Optional<std::string> OriginPolicyUI::GetErrorPageAsHTML(
    network::OriginPolicyState error_reason,
    content::NavigationHandle* handle) {
  DCHECK(handle);
  std::unique_ptr<SecurityInterstitialPage> page(GetErrorPageImpl(
      error_reason, handle->GetWebContents(), handle->GetURL()));
  std::string html = page->GetHTMLContents();

  // The page object is "associated" with the web contents, and this is how
  // the interstitial infrastructure will find this instance again.
  security_interstitials::SecurityInterstitialTabHelper::AssociateBlockingPage(
      handle->GetWebContents(), handle->GetNavigationId(), std::move(page));

  return html;
}

SecurityInterstitialPage* OriginPolicyUI::GetBlockingPage(
    network::OriginPolicyState error_reason,
    content::WebContents* web_contents,
    const GURL& url) {
  return GetErrorPageImpl(error_reason, web_contents, url).release();
}

}  // namespace security_interstitials
