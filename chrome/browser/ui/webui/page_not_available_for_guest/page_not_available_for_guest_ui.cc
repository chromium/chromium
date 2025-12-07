// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/page_not_available_for_guest/page_not_available_for_guest_ui.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/content/common/web_ui_constants.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

void CreateAndAddHTMLSource(Profile* profile, const std::string& host_name) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::CreateAndAdd(profile, host_name);

  std::u16string page_title;
  if (host_name == chrome::kChromeUIBookmarksHost) {
    page_title = l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_TITLE);
  } else if (host_name == chrome::kChromeUIHistoryHost) {
    page_title = l10n_util::GetStringUTF16(IDS_HISTORY_TITLE);
  } else if (host_name == chrome::kChromeUIExtensionsHost) {
    page_title = l10n_util::GetStringUTF16(IDS_EXTENSIONS_TOOLBAR_TITLE);
  } else if (host_name == password_manager::kChromeUIPasswordManagerHost) {
#if !BUILDFLAG(IS_ANDROID)
    page_title = l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_UI_TITLE);
#endif
  } else {
    page_title = base::UTF8ToUTF16(host_name);
  }

  source->AddString("pageTitle", page_title);

// TODO(crbug.com/391777809): Make the message available on desktop android
// without adding unused strings.
#if BUILDFLAG(IS_ANDROID)
  source->AddString("pageHeading", "");
#else
  std::u16string page_heading = l10n_util::GetStringFUTF16(
      IDS_PAGE_NOT_AVAILABLE_FOR_GUEST_HEADING, page_title);
  source->AddString("pageHeading", page_heading);
#endif

  source->SetDefaultResource(IDR_PAGE_NOT_AVAILABLE_FOR_GUEST_APP_HTML);
}

}  // namespace

PageNotAvailableForGuestUI::PageNotAvailableForGuestUI(
    content::WebUI* web_ui,
    const std::string& host_name)
    : WebUIController(web_ui) {
  CreateAndAddHTMLSource(Profile::FromWebUI(web_ui), host_name);
}
