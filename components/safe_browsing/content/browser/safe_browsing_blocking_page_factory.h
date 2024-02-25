// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_BLOCKING_PAGE_FACTORY_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_BLOCKING_PAGE_FACTORY_H_

#include "components/safe_browsing/content/browser/safe_browsing_blocking_page.h"

class GURL;

namespace content {
class WebContents;
}

namespace security_interstitials {
class SecurityInterstitialPage;
}

namespace safe_browsing {

class BaseUIManager;

// Factory for creating SafeBrowsingBlockingPage.
class SafeBrowsingBlockingPageFactory {
 public:
  virtual ~SafeBrowsingBlockingPageFactory() = default;

  virtual SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      BaseUIManager* ui_manager,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources,
      bool should_trigger_reporting,
      std::optional<base::TimeTicks> blocked_page_shown_timestamp) = 0;

#if !BUILDFLAG(IS_ANDROID)
  virtual security_interstitials::SecurityInterstitialPage*
  CreateEnterpriseWarnPage(
      BaseUIManager* ui_manager,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources) = 0;

  virtual security_interstitials::SecurityInterstitialPage*
  CreateEnterpriseBlockPage(
      BaseUIManager* ui_manager,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources) = 0;
#endif
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_BLOCKING_PAGE_FACTORY_H_
