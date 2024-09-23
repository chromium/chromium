// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TPCD_ENTERPRISE_REPORTING_ENTERPRISE_REPORTING_TAB_HELPER_H_
#define COMPONENTS_TPCD_ENTERPRISE_REPORTING_ENTERPRISE_REPORTING_TAB_HELPER_H_

#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace tpcd::enterprise_reporting {

class EnterpriseReportingTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<EnterpriseReportingTabHelper> {
 public:
  EnterpriseReportingTabHelper(const EnterpriseReportingTabHelper&) = delete;
  EnterpriseReportingTabHelper& operator=(const EnterpriseReportingTabHelper&) =
      delete;
  ~EnterpriseReportingTabHelper() override;

  // content::WebContentsObserver implementation.
  void OnCookiesAccessed(content::RenderFrameHost* render_frame_host,
                         const content::CookieAccessDetails& details) override;

  void OnCookiesAccessed(content::NavigationHandle* navigation_handle,
                         const content::CookieAccessDetails& details) override;

 private:
  explicit EnterpriseReportingTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<EnterpriseReportingTabHelper>;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace tpcd::enterprise_reporting

#endif  // COMPONENTS_TPCD_ENTERPRISE_REPORTING_ENTERPRISE_REPORTING_TAB_HELPER_H_
