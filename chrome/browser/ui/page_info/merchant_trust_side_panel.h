// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_INFO_MERCHANT_TRUST_SIDE_PANEL_H_
#define CHROME_BROWSER_UI_PAGE_INFO_MERCHANT_TRUST_SIDE_PANEL_H_

class GURL;

namespace content {
class WebContents;
}  // namespace content

static const char kMerchantTrustContextParameterName[] = "s";
static const char kMerchantTrustContextParameterValue[] = "CHROME_SIDE_PANEL";

// Implemented by merchant_trust_side_panel_coordinator.cc in ui/views.
void ShowMerchantTrustSidePanel(content::WebContents* web_contents,
                                const GURL& merchant_reviews_url);
#endif  // CHROME_BROWSER_UI_PAGE_INFO_MERCHANT_TRUST_SIDE_PANEL_H_
