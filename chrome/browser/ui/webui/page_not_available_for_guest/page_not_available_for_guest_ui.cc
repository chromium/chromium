// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/page_not_available_for_guest/page_not_available_for_guest_ui.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

content::WebUIDataSource* CreateHTMLSource(Profile* profile,
                                           const std::string& host_name) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(host_name);

  base::string16 page_title;
  if (host_name == chrome::kChromeUIBookmarksHost)
    page_title = l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_TITLE);
  else if (host_name == chrome::kChromeUIHistoryHost)
    page_title = l10n_util::GetStringUTF16(IDS_HISTORY_TITLE);
  else if (host_name == chrome::kChromeUIExtensionsHost)
    page_title = l10n_util::GetStringUTF16(IDS_EXTENSIONS_TOOLBAR_TITLE);
  else
    page_title = base::UTF8ToUTF16(host_name);

  source->AddString("pageTitle", page_title);
  base::string16 page_heading = l10n_util::GetStringFUTF16(
      IDS_PAGE_NOT_AVAILABLE_FOR_GUEST_HEADING, page_title);
  source->AddString("pageHeading", page_heading);

  source->SetDefaultResource(IDR_PAGE_NOT_AVAILABLE_FOR_GUEST_APP_HTML);

  return source;
}

}  // namespace

PageNotAvailableForGuestUI::PageNotAvailableForGuestUI(
    content::WebUI* web_ui,
    const std::string& host_name)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateHTMLSource(profile, host_name));
}
