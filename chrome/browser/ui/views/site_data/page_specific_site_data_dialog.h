// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SITE_DATA_PAGE_SPECIFIC_SITE_DATA_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_SITE_DATA_PAGE_SPECIFIC_SITE_DATA_DIALOG_H_

#include "components/content_settings/core/common/content_settings.h"
#include "ui/base/interaction/element_identifier.h"
#include "url/origin.h"

namespace views {
class Widget;
}  // namespace views

namespace content {
class WebContents;
}  // namespace content

namespace {
class PageSpecificSiteDataDialogModelDelegate;
}  // namespace

// |PageSpecificSiteDataDialogSite| represents a site entry in page specific
// site data dialog. It contains information about origin, the effective content
// setting and the partitioned state.
struct PageSpecificSiteDataDialogSite {
  url::Origin origin;
  ContentSetting setting;
  bool is_fully_partitioned;
};

namespace test {

// API to expose dialog delegate for unit tests. It provides access to internal
// methods for testing. API creates and holds a reference to a dialog delegate.
class PageSpecificSiteDataDialogTestApi {
 public:
  explicit PageSpecificSiteDataDialogTestApi(
      content::WebContents* web_contents);
  ~PageSpecificSiteDataDialogTestApi();

  std::vector<PageSpecificSiteDataDialogSite> GetAllSites();

 private:
  std::unique_ptr<PageSpecificSiteDataDialogModelDelegate> delegate_;
};

}  // namespace test

DECLARE_ELEMENT_IDENTIFIER_VALUE(kPageSpecificSiteDataDialogRow);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kPageSpecificSiteDataDialogFirstPartySection);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kPageSpecificSiteDataDialogThirdPartySection);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kPageSpecificSiteDataDialogEmptyStateLabel);

views::Widget* ShowPageSpecificSiteDataDialog(
    content::WebContents* web_contents);

#endif  // CHROME_BROWSER_UI_VIEWS_SITE_DATA_PAGE_SPECIFIC_SITE_DATA_DIALOG_H_
