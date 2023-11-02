// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SITE_DATA_PAGE_SPECIFIC_SITE_DATA_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_SITE_DATA_PAGE_SPECIFIC_SITE_DATA_DIALOG_H_

#include "ui/base/interaction/element_identifier.h"

namespace views {
class Widget;
}  // namespace views

namespace content {
class WebContents;
}  // namespace content

DECLARE_ELEMENT_IDENTIFIER_VALUE(kPageSpecificSiteDataDialogRow);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kPageSpecificSiteDataDialogFirstPartySection);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kPageSpecificSiteDataDialogThirdPartySection);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kPageSpecificSiteDataDialogEmptyStateLabel);

views::Widget* ShowPageSpecificSiteDataDialog(
    content::WebContents* web_contents);

#endif  // CHROME_BROWSER_UI_VIEWS_SITE_DATA_PAGE_SPECIFIC_SITE_DATA_DIALOG_H_
