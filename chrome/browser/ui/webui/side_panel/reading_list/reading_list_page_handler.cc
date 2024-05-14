// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/reading_list/reading_list_page_handler.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/side_panel/reading_list/reading_list_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "url/gurl.h"

namespace {

// Sorter function that orders ReadingListEntries by their update time.
bool EntrySorter(const reading_list::mojom::ReadLaterEntryPtr& rhs,
                 const reading_list::mojom::ReadLaterEntryPtr& lhs) {
  return rhs->update_time > lhs->update_time;
}

// Converts |time| to the number of microseconds since Jan 1st 1970.
// This matches the function used in the ReadingListEntry implementation.
int64_t TimeToUS(const base::Time& time) {
  return (time - base::Time::UnixEpoch()).InMicroseconds();
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
        browser_->OpenURL(params, /*navigation_handle_callback=*/{});
        break;
      }

      case IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW: {
        content::OpenURLParams params(url_, content::Referrer(),
                                      WindowOpenDisposition::NEW_WINDOW,
                                      ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
        browser_->OpenURL(params, /*navigation_handle_callback=*/{});
        break;
      }

      case IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD: {
        content::OpenURLParams params(url_, content::Referrer(),
                                      WindowOpenDisposition::OFF_THE_RECORD,
                                      ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
        browser_->OpenURL(params, /*navigation_handle_callback=*/{});
        break;
      }

      case kMarkAsRead:
        reading_list_model_->SetReadStatusIfExists(url_, true);
        break;
      case kMarkAsUnread:
        reading_list_model_->SetReadStatusIfExists(url_, false);
        break;
      case kDelete:
        reading_list_model_->RemoveEntryByURL(url_, FROM_HERE);
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  bool IsCommandIdEnabled(int command_id) const override {
    PrefService* prefs = browser_->profile()->GetPrefs();
    policy::IncognitoModeAvailability incognito_avail =
        IncognitoModePrefs::GetAvailability(prefs);
    switch (command_id) {
      case IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD:
        return !browser_->profile()->IsOffTheRecord() &&
               incognito_avail != policy::IncognitoModeAvailability::kDisabled;
    }
    return true;
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

ReadingListPageHandler::ReadingListPageHandler(
    mojo::PendingReceiver<reading_list::mojom::PageHandler> receiver,
    mojo::PendingRemote<reading_list::mojom::Page> page,
    ReadingListUI* reading_list_ui,
    content::WebUI* web_ui)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      reading_list_ui_(reading_list_ui),
      web_ui_(web_ui),
      web_contents_(web_ui->GetWebContents()),
      clock_(base::DefaultClock::GetInstance()) {
  Profile* profile = Profile::FromWebUI(web_ui);
  DCHECK(profile);

  reading_list_model_ = ReadingListModelFactory::GetForBrowserContext(profile);
  reading_list_model_scoped_observation_.Observe(reading_list_model_.get());
}

ReadingListPageHandler::~ReadingListPageHandler() = default;

void ReadingListPageHandler::GetReadLaterEntries(
    GetReadLaterEntriesCallback callback) {
  std::move(callback).Run(CreateReadLaterEntriesByStatusData());
}

void ReadingListPageHandler::OpenURL(
    const GURL& url,
    bool mark_as_read,
    ui::mojom::ClickModifiersPtr click_modifiers) {
  Browser* browser = chrome::FindLastActive();
  if (!browser)
    return;

  // Open in active tab if the user is on the NTP.
  WindowOpenDisposition open_location = ui::DispositionFromClick(
      click_modifiers->middle_button, click_modifiers->alt_key,
      click_modifiers->ctrl_key, click_modifiers->meta_key,
      click_modifiers->shift_key);

  content::OpenURLParams params(url, content::Referrer(), open_location,
                                ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
  browser->OpenURL(params, /*navigation_handle_callback=*/{});

  scoped_refptr<const ReadingListEntry> entry =
      reading_list_model_->GetEntryByURL(url);
  if (entry) {
    base::RecordAction(base::UserMetricsAction(
        entry->IsRead() ? "DesktopReadingList.Navigation.FromReadList"
                        : "DesktopReadingList.Navigation.FromUnreadList"));
  }

  base::RecordAction(
      base::UserMetricsAction("SidePanel.ReadingList.Navigation"));
  RecordBookmarkLaunch(
      BookmarkLaunchLocation::kSidePanelPendingList,
      profile_metrics::GetBrowserProfileType(Profile::FromWebUI(web_ui_)));
}

void ReadingListPageHandler::UpdateReadStatus(const GURL& url, bool read) {
  reading_list_model_->SetReadStatusIfExists(url, read);
  base::RecordAction(
      base::UserMetricsAction(read ? "DesktopReadingList.MarkAsRead"
                                   : "DesktopReadingList.MarkAsUnread"));
}

void ReadingListPageHandler::MarkCurrentTabAsRead() {
  Browser* browser = chrome::FindLastActive();
  if (!browser)
    return;

  chrome::MarkCurrentTabAsReadInReadLater(browser);
  base::RecordAction(base::UserMetricsAction("DesktopReadingList.MarkAsRead"));
}

void ReadingListPageHandler::AddCurrentTab() {
  Browser* browser = chrome::FindLastActive();
  if (!browser)
    return;

  chrome::MoveCurrentTabToReadLater(browser);
  reading_list_model_->MarkAllSeen();

  base::RecordAction(
      base::UserMetricsAction("SidePanel.ReadingList.AddCurrentPage"));
}

void ReadingListPageHandler::RemoveEntry(const GURL& url) {
  reading_list_model_->RemoveEntryByURL(url, FROM_HERE);
  base::RecordAction(base::UserMetricsAction("DesktopReadingList.RemoveItem"));
}

void ReadingListPageHandler::ShowContextMenuForURL(const GURL& url,
                                                   int32_t x,
                                                   int32_t y) {
  auto embedder = reading_list_ui_->embedder();
  Browser* browser = chrome::FindLastActive();
  if (embedder)
    embedder->ShowContextMenu(gfx::Point(x, y),
                              std::make_unique<ReadLaterItemContextMenu>(
                                  browser, reading_list_model_, url));
}

void ReadingListPageHandler::UpdateCurrentPageActionButtonState() {
  page_->CurrentPageActionButtonStateChanged(current_page_action_button_state_);
}

void ReadingListPageHandler::ShowUI() {
  auto embedder = reading_list_ui_->embedder();
  if (embedder) {
    embedder->ShowUI();
  }
}

void ReadingListPageHandler::CloseUI() {
  auto embedder = reading_list_ui_->embedder();
  if (embedder)
    embedder->CloseUI();
}

void ReadingListPageHandler::ReadingListModelCompletedBatchUpdates(
    const ReadingListModel* model) {
  DCHECK(model == reading_list_model_);
  if (web_contents_->GetVisibility() == content::Visibility::HIDDEN)
    return;
  page_->ItemsChanged(CreateReadLaterEntriesByStatusData());
  UpdateCurrentPageActionButton();
  reading_list_model_->MarkAllSeen();
}

void ReadingListPageHandler::ReadingListModelBeingDeleted(
    const ReadingListModel* model) {
  DCHECK(model == reading_list_model_);
  DCHECK(reading_list_model_scoped_observation_.IsObservingSource(
      reading_list_model_.get()));
  reading_list_model_scoped_observation_.Reset();
  reading_list_model_ = nullptr;
}

void ReadingListPageHandler::ReadingListDidApplyChanges(
    ReadingListModel* model) {
  DCHECK(model == reading_list_model_);
  if (web_contents_->GetVisibility() == content::Visibility::HIDDEN ||
      reading_list_model_->IsPerformingBatchUpdates()) {
    return;
  }
  page_->ItemsChanged(CreateReadLaterEntriesByStatusData());
  UpdateCurrentPageActionButton();
  reading_list_model_->MarkAllSeen();
}

const std::optional<GURL> ReadingListPageHandler::GetActiveTabURL() {
  if (active_tab_url_)
    return active_tab_url_.value();
  Browser* browser = chrome::FindLastActive();
  if (browser) {
    return chrome::GetURLToBookmark(
        browser->tab_strip_model()->GetActiveWebContents());
  }
  return std::nullopt;
}

void ReadingListPageHandler::SetActiveTabURL(const GURL& url) {
  if (active_tab_url_ && active_tab_url_.value() == url)
    return;

  active_tab_url_ = url;
  UpdateCurrentPageActionButton();
}

reading_list::mojom::ReadLaterEntryPtr ReadingListPageHandler::GetEntryData(
    const ReadingListEntry* entry) {
  auto entry_data = reading_list::mojom::ReadLaterEntry::New();

  entry_data->title = entry->Title();
  entry_data->url = entry->URL();
  entry_data->display_url = base::UTF16ToUTF8(url_formatter::FormatUrl(
      entry->URL(),
      url_formatter::kFormatUrlOmitDefaults |
          url_formatter::kFormatUrlOmitHTTPS |
          url_formatter::kFormatUrlOmitTrivialSubdomains |
          url_formatter::kFormatUrlTrimAfterHost,
      base::UnescapeRule::NORMAL, nullptr, nullptr, nullptr));
  entry_data->update_time = entry->UpdateTime();
  entry_data->read = entry->IsRead();
  entry_data->display_time_since_update =
      GetTimeSinceLastUpdate(entry->UpdateTime());

  return entry_data;
}

reading_list::mojom::ReadLaterEntriesByStatusPtr
ReadingListPageHandler::CreateReadLaterEntriesByStatusData() {
  auto entries = reading_list::mojom::ReadLaterEntriesByStatus::New();

  for (const auto& url : reading_list_model_->GetKeys()) {
    scoped_refptr<const ReadingListEntry> entry =
        reading_list_model_->GetEntryByURL(url);
    DCHECK(entry);
    if (entry->IsRead()) {
      entries->read_entries.push_back(GetEntryData(entry.get()));
    } else {
      entries->unread_entries.push_back(GetEntryData(entry.get()));
    }
  }

  std::sort(entries->read_entries.begin(), entries->read_entries.end(),
            EntrySorter);
  std::sort(entries->unread_entries.begin(), entries->unread_entries.end(),
            EntrySorter);

  return entries;
}

std::string ReadingListPageHandler::GetTimeSinceLastUpdate(
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

void ReadingListPageHandler::UpdateCurrentPageActionButton() {
  if (web_contents_->GetVisibility() == content::Visibility::HIDDEN ||
      Profile::FromWebUI(web_ui_)->IsGuestSession())
    return;

  const std::optional<GURL> url = GetActiveTabURL();
  if (!url.has_value())
    return;

  reading_list::mojom::CurrentPageActionButtonState new_state;
  if (!reading_list_model_->IsUrlSupported(url.value())) {
    new_state = reading_list::mojom::CurrentPageActionButtonState::kDisabled;
  } else if ((reading_list_model_->GetEntryByURL(url.value()) &&
              !reading_list_model_->GetEntryByURL(url.value())->IsRead())) {
    new_state = reading_list::mojom::CurrentPageActionButtonState::kMarkAsRead;
  } else {
    new_state = reading_list::mojom::CurrentPageActionButtonState::kAdd;
  }
  if (current_page_action_button_state_ != new_state) {
    current_page_action_button_state_ = new_state;
    page_->CurrentPageActionButtonStateChanged(
        current_page_action_button_state_);
  }
}

std::unique_ptr<ui::SimpleMenuModel>
ReadingListPageHandler::GetItemContextMenuModelForTesting(
    Browser* browser,
    ReadingListModel* reading_list_model,
    GURL url) {
  return std::make_unique<ReadLaterItemContextMenu>(browser, reading_list_model,
                                                    url);
}
