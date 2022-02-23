// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/read_later/read_later_page_handler.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/read_later/reading_list_model_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/read_later/read_later_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/base/window_open_disposition.h"
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
    const GURL site_origin =
        web_contents->GetLastCommittedURL().DeprecatedGetOriginAsURL();
    // These are also the NTP urls checked for showing the bookmark bar on the
    // NTP.
    if (site_origin ==
            GURL(chrome::kChromeUINewTabURL).DeprecatedGetOriginAsURL() ||
        site_origin ==
            GURL(chrome::kChromeUINewTabPageURL).DeprecatedGetOriginAsURL()) {
      return true;
    }
  }
  return false;
}

class ReadLaterItemContextMenu : public ui::SimpleMenuModel,
                                 public ui::SimpleMenuModel::Delegate {
 public:
  ReadLaterItemContextMenu(Browser* browser,
                           ReadingListModel* reading_list_model,
                           GURL url)
      : ui::SimpleMenuModel(this),
        browser_(browser),
        reading_list_model_(reading_list_model),
        url_(url) {
    // Context menus have bookmark strings to keep consistent with Bookmark tab
    // in the side panel.
    AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB,
                        IDS_BOOKMARK_BAR_OPEN_IN_NEW_TAB);
    AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW,
                        IDS_BOOKMARK_BAR_OPEN_IN_NEW_WINDOW);
    AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD,
                        IDS_BOOKMARK_BAR_OPEN_INCOGNITO);
    AddSeparator(ui::NORMAL_SEPARATOR);

    if (reading_list_model->GetEntryByURL(url)->IsRead()) {
      AddItemWithStringId(kMarkAsUnread,
                          IDS_READ_LATER_CONTEXT_MENU_MARK_AS_UNREAD);
    } else {
      AddItemWithStringId(kMarkAsRead,
                          IDS_READ_LATER_CONTEXT_MENU_MARK_AS_READ);
    }
    AddItemWithStringId(kDelete, IDS_READ_LATER_CONTEXT_MENU_DELETE);
  }
  ~ReadLaterItemContextMenu() override = default;

  void ExecuteCommand(int command_id, int event_flags) override {
    switch (command_id) {
      case IDC_CONTENT_CONTEXT_OPENLINKNEWTAB: {
        content::OpenURLParams params(url_, content::Referrer(),
                                      WindowOpenDisposition::NEW_BACKGROUND_TAB,
                                      ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
        browser_->OpenURL(params);
        break;
      }

      case IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW: {
        content::OpenURLParams params(url_, content::Referrer(),
                                      WindowOpenDisposition::NEW_WINDOW,
                                      ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
        browser_->OpenURL(params);
        break;
      }

      case IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD: {
        content::OpenURLParams params(url_, content::Referrer(),
                                      WindowOpenDisposition::OFF_THE_RECORD,
                                      ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
        browser_->OpenURL(params);
        break;
      }

      case kMarkAsRead:
        reading_list_model_->SetReadStatus(url_, true);
        break;
      case kMarkAsUnread:
        reading_list_model_->SetReadStatus(url_, false);
        break;
      case kDelete:
        reading_list_model_->RemoveEntryByURL(url_);
        break;
      default:
        NOTREACHED();
        break;
    }
  }

 private:
  enum MenuCommands {
    kMarkAsRead,
    kMarkAsUnread,
    kDelete,
  };
  const raw_ptr<Browser> browser_;
  raw_ptr<ReadingListModel> reading_list_model_;
  GURL url_;
};

}  // namespace

ReadLaterPageHandler::ReadLaterPageHandler(
    mojo::PendingReceiver<read_later::mojom::PageHandler> receiver,
    mojo::PendingRemote<read_later::mojom::Page> page,
    ReadLaterUI* read_later_ui,
    content::WebUI* web_ui)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      read_later_ui_(read_later_ui),
      web_ui_(web_ui),
      web_contents_(web_ui->GetWebContents()),
      clock_(base::DefaultClock::GetInstance()) {
  Profile* profile = Profile::FromWebUI(web_ui);
  DCHECK(profile);

  reading_list_model_ = ReadingListModelFactory::GetForBrowserContext(profile);
  reading_list_model_scoped_observation_.Observe(reading_list_model_.get());
}

ReadLaterPageHandler::~ReadLaterPageHandler() = default;

void ReadLaterPageHandler::GetReadLaterEntries(
    GetReadLaterEntriesCallback callback) {
  std::move(callback).Run(CreateReadLaterEntriesByStatusData());
}

void ReadLaterPageHandler::OpenURL(
    const GURL& url,
    bool mark_as_read,
    ui::mojom::ClickModifiersPtr click_modifiers) {
  Browser* browser = chrome::FindLastActive();
  if (!browser)
    return;

  const bool side_panel_enabled =
      base::FeatureList::IsEnabled(features::kSidePanel);

  // Open in active tab if the user is on the NTP.
  WindowOpenDisposition open_location =
      IsActiveTabNTP(browser) ? WindowOpenDisposition::CURRENT_TAB
                              : WindowOpenDisposition::NEW_FOREGROUND_TAB;
  if (side_panel_enabled) {
    open_location = ui::DispositionFromClick(
        click_modifiers->middle_button, click_modifiers->alt_key,
        click_modifiers->ctrl_key, click_modifiers->meta_key,
        click_modifiers->shift_key);
  }

  content::OpenURLParams params(url, content::Referrer(), open_location,
                                ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
  browser->OpenURL(params);

  const ReadingListEntry* entry = reading_list_model_->GetEntryByURL(url);
  if (entry) {
    base::RecordAction(base::UserMetricsAction(
        entry->IsRead() ? "DesktopReadingList.Navigation.FromReadList"
                        : "DesktopReadingList.Navigation.FromUnreadList"));
  }

  if (mark_as_read && !side_panel_enabled)
    reading_list_model_->SetReadStatus(url, true);

  base::RecordAction(base::UserMetricsAction(
      side_panel_enabled ? "SidePanel.ReadingList.Navigation"
                         : "ReadingList.Dialog.Navigation"));
  RecordBookmarkLaunch(
      side_panel_enabled ? BOOKMARK_LAUNCH_LOCATION_SIDE_PANEL_READING_LIST
                         : BOOKMARK_LAUNCH_LOCATION_READING_LIST_DIALOG,
      profile_metrics::GetBrowserProfileType(Profile::FromWebUI(web_ui_)));
}

void ReadLaterPageHandler::UpdateReadStatus(const GURL& url, bool read) {
  reading_list_model_->SetReadStatus(url, read);
  base::RecordAction(
      base::UserMetricsAction(read ? "DesktopReadingList.MarkAsRead"
                                   : "DesktopReadingList.MarkAsUnread"));
}

void ReadLaterPageHandler::AddCurrentTab() {
  Browser* browser = chrome::FindLastActive();
  if (!browser)
    return;

  chrome::MoveCurrentTabToReadLater(browser);
  reading_list_model_->MarkAllSeen();

  base::RecordAction(
      base::UserMetricsAction(base::FeatureList::IsEnabled(features::kSidePanel)
                                  ? "SidePanel.ReadingList.AddCurrentPage"
                                  : "ReadingList.Dialog.AddCurrentPage"));
}

void ReadLaterPageHandler::RemoveEntry(const GURL& url) {
  reading_list_model_->RemoveEntryByURL(url);
  base::RecordAction(base::UserMetricsAction("DesktopReadingList.RemoveItem"));
}

void ReadLaterPageHandler::ShowContextMenuForURL(const GURL& url,
                                                 int32_t x,
                                                 int32_t y) {
  auto embedder = read_later_ui_->embedder();
  Browser* browser = chrome::FindLastActive();
  if (embedder)
    embedder->ShowContextMenu(gfx::Point(x, y),
                              std::make_unique<ReadLaterItemContextMenu>(
                                  browser, reading_list_model_, url));
}

void ReadLaterPageHandler::UpdateCurrentPageActionButtonState() {
  page_->CurrentPageActionButtonStateChanged(current_page_action_button_state_);
}

void ReadLaterPageHandler::ShowUI() {
  auto embedder = read_later_ui_->embedder();
  if (embedder) {
    embedder->ShowUI();
    if (!base::FeatureList::IsEnabled(features::kSidePanel))
      UpdateCurrentPageActionButton();
  }
}

void ReadLaterPageHandler::CloseUI() {
  auto embedder = read_later_ui_->embedder();
  if (embedder)
    embedder->CloseUI();
}

void ReadLaterPageHandler::ReadingListModelCompletedBatchUpdates(
    const ReadingListModel* model) {
  DCHECK(model == reading_list_model_);
  if (web_contents_->GetVisibility() == content::Visibility::HIDDEN)
    return;
  page_->ItemsChanged(CreateReadLaterEntriesByStatusData());
  UpdateCurrentPageActionButton();
  reading_list_model_->MarkAllSeen();
}

void ReadLaterPageHandler::ReadingListModelBeingDeleted(
    const ReadingListModel* model) {
  DCHECK(model == reading_list_model_);
  DCHECK(reading_list_model_scoped_observation_.IsObservingSource(
      reading_list_model_.get()));
  reading_list_model_scoped_observation_.Reset();
  reading_list_model_ = nullptr;
}

void ReadLaterPageHandler::ReadingListDidApplyChanges(ReadingListModel* model) {
  DCHECK(model == reading_list_model_);
  if (web_contents_->GetVisibility() == content::Visibility::HIDDEN ||
      reading_list_model_->IsPerformingBatchUpdates()) {
    return;
  }
  page_->ItemsChanged(CreateReadLaterEntriesByStatusData());
  UpdateCurrentPageActionButton();
  reading_list_model_->MarkAllSeen();
}

const absl::optional<GURL> ReadLaterPageHandler::GetActiveTabURL() {
  if (active_tab_url_)
    return active_tab_url_.value();
  Browser* browser = chrome::FindLastActive();
  if (browser) {
    return chrome::GetURLToBookmark(
        browser->tab_strip_model()->GetActiveWebContents());
  }
  return absl::nullopt;
}

void ReadLaterPageHandler::SetActiveTabURL(const GURL& url) {
  if (active_tab_url_ && active_tab_url_.value() == url)
    return;

  active_tab_url_ = url;
  UpdateCurrentPageActionButton();
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
      base::Microseconds(now - last_update_time);
  return base::UTF16ToUTF8(
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                             ui::TimeFormat::LENGTH_SHORT, elapsed_time));
}

void ReadLaterPageHandler::UpdateCurrentPageActionButton() {
  if (web_contents_->GetVisibility() == content::Visibility::HIDDEN ||
      Profile::FromWebUI(web_ui_)->IsGuestSession())
    return;

  const absl::optional<GURL> url = GetActiveTabURL();
  if (!url.has_value())
    return;

  read_later::mojom::CurrentPageActionButtonState new_state;
  if (!reading_list_model_->IsUrlSupported(url.value()) ||
      (reading_list_model_->GetEntryByURL(url.value()) &&
       !reading_list_model_->GetEntryByURL(url.value())->IsRead())) {
    new_state = read_later::mojom::CurrentPageActionButtonState::kDisabled;
  } else {
    new_state = read_later::mojom::CurrentPageActionButtonState::kAdd;
  }
  if (current_page_action_button_state_ != new_state) {
    current_page_action_button_state_ = new_state;
    page_->CurrentPageActionButtonStateChanged(
        current_page_action_button_state_);
  }
}
