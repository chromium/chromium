// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/read_later/read_later_page_handler.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/read_later/reading_list_model_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/read_later/read_later_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/time_format.h"
#include "url/gurl.h"

namespace {

// Sorter function that orders ReadingListEntries by their update time.
bool EntrySorter(const read_later::mojom::ReadLaterEntryPtr& rhs,
                 const read_later::mojom::ReadLaterEntryPtr& lhs) {
  return rhs->update_time > lhs->update_time;
}

// Converts |time| to the number of microseconds since Jan 1st 1970.
// This matches the function used in the ReadingListEntry implementation.
int64_t TimeToUS(const base::Time& time) {
  return (time - base::Time::UnixEpoch()).InMicroseconds();
}

bool IsActiveTabNTP(Browser* browser) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (web_contents) {
    const GURL site_origin = web_contents->GetLastCommittedURL().GetOrigin();
    // These are also the NTP urls checked for showing the bookmark bar on the
    // NTP.
    if (site_origin == GURL(chrome::kChromeUINewTabURL).GetOrigin() ||
        site_origin == GURL(chrome::kChromeUINewTabPageURL).GetOrigin()) {
      return true;
    }
  }
  return false;
}

}  // namespace

ReadLaterPageHandler::ReadLaterPageHandler(
    mojo::PendingReceiver<read_later::mojom::PageHandler> receiver,
    mojo::PendingRemote<read_later::mojom::Page> page,
    ReadLaterUI* read_later_ui,
    content::WebUI* web_ui)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      read_later_ui_(read_later_ui),
      clock_(base::DefaultClock::GetInstance()) {
  Profile* profile = Profile::FromWebUI(web_ui);
  DCHECK(profile);

  reading_list_model_ = ReadingListModelFactory::GetForBrowserContext(profile);
}

ReadLaterPageHandler::~ReadLaterPageHandler() = default;

void ReadLaterPageHandler::GetReadLaterEntries(
    GetReadLaterEntriesCallback callback) {
  std::move(callback).Run(CreateReadLaterEntriesByStatusData());
}

void ReadLaterPageHandler::OpenSavedEntry(const GURL& url) {
  Browser* browser = chrome::FindLastActive();
  if (!browser)
    return;

  // Open in active tab if the user is on the NTP.
  WindowOpenDisposition open_location =
      IsActiveTabNTP(browser) ||
              base::FeatureList::IsEnabled(features::kSidePanel)
          ? WindowOpenDisposition::CURRENT_TAB
          : WindowOpenDisposition::NEW_FOREGROUND_TAB;

  content::OpenURLParams params(url, content::Referrer(), open_location,
                                ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
  browser->OpenURL(params);
  reading_list_model_->SetReadStatus(url, true);
}

void ReadLaterPageHandler::UpdateReadStatus(const GURL& url, bool read) {
  reading_list_model_->SetReadStatus(url, read);
  page_->ItemsChanged(CreateReadLaterEntriesByStatusData());
}

void ReadLaterPageHandler::RemoveEntry(const GURL& url) {
  reading_list_model_->RemoveEntryByURL(url);
  page_->ItemsChanged(CreateReadLaterEntriesByStatusData());
}

void ReadLaterPageHandler::ShowUI() {
  auto embedder = read_later_ui_->embedder();
  if (embedder)
    embedder->ShowUI();
}

void ReadLaterPageHandler::CloseUI() {
  auto embedder = read_later_ui_->embedder();
  if (embedder)
    embedder->CloseUI();
}

read_later::mojom::ReadLaterEntryPtr ReadLaterPageHandler::GetEntryData(
    const ReadingListEntry* entry) {
  auto entry_data = read_later::mojom::ReadLaterEntry::New();

  entry_data->title = entry->Title();
  entry_data->url = entry->URL();
  entry_data->display_url = base::UTF16ToUTF8(url_formatter::FormatUrl(
      entry->URL(),
      url_formatter::kFormatUrlOmitDefaults |
          url_formatter::kFormatUrlOmitHTTPS |
          url_formatter::kFormatUrlOmitTrivialSubdomains |
          url_formatter::kFormatUrlTrimAfterHost,
      net::UnescapeRule::NORMAL, nullptr, nullptr, nullptr));
  entry_data->update_time = entry->UpdateTime();
  entry_data->read = entry->IsRead();
  entry_data->display_time_since_update =
      GetTimeSinceLastUpdate(entry->UpdateTime());

  return entry_data;
}

read_later::mojom::ReadLaterEntriesByStatusPtr
ReadLaterPageHandler::CreateReadLaterEntriesByStatusData() {
  auto entries = read_later::mojom::ReadLaterEntriesByStatus::New();

  for (const auto& url : reading_list_model_->Keys()) {
    const ReadingListEntry* entry = reading_list_model_->GetEntryByURL(url);
    DCHECK(entry);
    if (entry->IsRead()) {
      entries->read_entries.push_back(GetEntryData(entry));
    } else {
      entries->unread_entries.push_back(GetEntryData(entry));
    }
  }

  std::sort(entries->read_entries.begin(), entries->read_entries.end(),
            EntrySorter);
  std::sort(entries->unread_entries.begin(), entries->unread_entries.end(),
            EntrySorter);

  return entries;
}

std::string ReadLaterPageHandler::GetTimeSinceLastUpdate(
    int64_t last_update_time) {
  const int64_t now = TimeToUS(clock_->Now());
  if (last_update_time > now)
    return std::string();
  const base::TimeDelta elapsed_time =
      base::TimeDelta::FromMicroseconds(now - last_update_time);
  return base::UTF16ToUTF8(
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                             ui::TimeFormat::LENGTH_SHORT, elapsed_time));
}
