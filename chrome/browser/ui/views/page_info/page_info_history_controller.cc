// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_history_controller.h"

#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/page_info/core/page_info_history_data_source.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"

PageInfoHistoryController::PageInfoHistoryController(
    content::WebContents* web_contents,
    const GURL& site_url)
    : web_contents_(web_contents), site_url_(site_url) {
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

void PageInfoHistoryController::UpdateRow(
    std::optional<base::Time> last_visit) {
  if (!container_tracker_.view())
    return;

  auto* container_view =
      static_cast<PageInfoMainView::ContainerView*>(container_tracker_.view());
  container_view->RemoveAllChildViews();
  if (last_visit.has_value()) {
    container_view->AddChildView(CreateHistoryButton(
        page_info::PageInfoHistoryDataSource::FormatLastVisitedTimestamp(
            last_visit.value())));
    container_view->Update();
  }
}

std::unique_ptr<views::View> PageInfoHistoryController::CreateHistoryButton(
    std::u16string last_visit) {
  // TODO(crbug.com/40808038): Use correct icons and strings (tooltip).
  auto button = std::make_unique<RichHoverButton>(
      base::BindRepeating(&PageInfoHistoryController::OpenHistoryPage,
                          weak_factory_.GetWeakPtr()),
      PageInfoViewFactory::GetHistoryIcon(),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_HISTORY), last_visit,

      /*tooltip_text=*/std::u16string(), std::u16string(),
      PageInfoViewFactory::GetLaunchIcon());
  button->SetID(PageInfoViewFactory::VIEW_ID_PAGE_INFO_HISTORY_BUTTON);
  return button;
}

void PageInfoHistoryController::OpenHistoryPage() {
  // TODO(crbug.com/40808038): Add test for destroring web content.
  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  chrome::ShowHistory(browser, site_url_.host());
}
