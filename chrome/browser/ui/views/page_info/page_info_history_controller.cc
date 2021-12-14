// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_history_controller.h"

#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/views/page_info/page_info_hover_button.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/page_info/core/page_info_history_data_source.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/view.h"

PageInfoHistoryController::PageInfoHistoryController(
    content::WebContents* web_contents,
    const GURL& site_url)
    : web_contents_(web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  history_data_source_ = std::make_unique<page_info::PageInfoHistoryDataSource>(
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      site_url);
}

PageInfoHistoryController::~PageInfoHistoryController() = default;

void PageInfoHistoryController::InitRow(views::View* container) {
  container_tracker_.SetView(container);
  history_data_source_->GetLastVisitedTimestamp(base::BindOnce(
      &PageInfoHistoryController::UpdateRow, weak_factory_.GetWeakPtr()));
}

void PageInfoHistoryController::UpdateRow(base::Time last_visit) {
  if (!container_tracker_.view())
    return;

  container_tracker_.view()->RemoveAllChildViews();
  if (!last_visit.is_null()) {
    container_tracker_.view()->AddChildView(CreateHistoryButton(
        page_info::PageInfoHistoryDataSource::FormatLastVisitedTimestamp(
            last_visit)));
  }
}

std::unique_ptr<views::View> PageInfoHistoryController::CreateHistoryButton(
    std::u16string last_visit) {
  // TODO(crbug.com/1275042): Use correct icons and strings (tooltip).
  return std::make_unique<PageInfoHoverButton>(
      base::BindRepeating(&PageInfoHistoryController::OpenHistoryPage,
                          weak_factory_.GetWeakPtr()),
      PageInfoViewFactory::GetSiteSettingsIcon(), IDS_PAGE_INFO_HISTORY,
      last_visit, PageInfoViewFactory::VIEW_ID_PAGE_INFO_HISTORY_BUTTON,
      /*tooltip_text=*/std::u16string(), std::u16string(),
      PageInfoViewFactory::GetLaunchIcon());
}

void PageInfoHistoryController::OpenHistoryPage() {
  // TODO(crbug.com/1275042): Open history page for the site.
  // TODO(crbug.com/1275042): Add test for destroring web content.
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  chrome::ShowHistory(browser);
}
