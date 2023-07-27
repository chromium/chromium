// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/custom_home_pages_table_model.h"

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/history/core/browser/history_service.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#endif

struct CustomHomePagesTableModel::Entry {
  Entry() : task_id(base::CancelableTaskTracker::kBadTaskId) {}

  // URL of the page.
  GURL url;

  // Page title.  If this is empty, we'll display the URL as the entry.
  std::u16string title;

  // If not |base::CancelableTaskTracker::kBadTaskId|, indicates we're loading
  // the title for the page.
  base::CancelableTaskTracker::TaskId task_id;
};

CustomHomePagesTableModel::CustomHomePagesTableModel(Profile* profile)
    : profile_(profile),
      observer_(nullptr),
      num_outstanding_title_lookups_(0) {}

CustomHomePagesTableModel::~CustomHomePagesTableModel() {
}

void CustomHomePagesTableModel::SetURLs(const std::vector<GURL>& urls) {
  entries_.resize(urls.size());
  for (size_t i = 0; i < urls.size(); ++i) {
    entries_[i].url = urls[i];
    entries_[i].title.erase();
  }
  LoadAllTitles();
}

/**
 * Move a number of existing entries to a new position, reordering the table.
 *
 * We determine the range of elements affected by the move, save the moved
 * elements, compact the remaining ones, and re-insert moved elements.
 * Expects |index_list| to be ordered ascending.
 */
void CustomHomePagesTableModel::MoveURLs(
    size_t insert_before,
    const std::vector<size_t>& index_list) {
  if (index_list.empty()) return;
  DCHECK(insert_before <= RowCount());

  // The range of elements that needs to be reshuffled is [ |first|, |last| ).
  size_t first = std::min(insert_before, index_list.front());
  size_t last = std::max(insert_before, index_list.back() + 1);

  // Save the dragged elements. Also, adjust insertion point if it is before a
  // dragged element.
  std::vector<Entry> moved_entries;
  for (size_t i = 0; i < index_list.size(); ++i) {
    moved_entries.push_back(entries_[index_list[i]]);
    if (index_list[i] == insert_before)
      insert_before++;
  }

  // Compact the range between beginning and insertion point, moving downwards.
  size_t skip_count = 0;
  for (size_t i = first; i < insert_before; ++i) {
    if (skip_count < index_list.size() && index_list[skip_count] == i)
      skip_count++;
    else
      entries_[i - skip_count] = entries_[i];
  }

  // Moving items down created a gap. We start compacting up after it.
  first = insert_before;
  insert_before -= skip_count;

  // Now compact up for elements after the insertion point.
  skip_count = 0;
  for (size_t i = last; i > first; --i) {
    const size_t index = i - 1;
    if (skip_count < index_list.size() &&
        index_list[index_list.size() - skip_count - 1] == index) {
      skip_count++;
    } else {
      entries_[index + skip_count] = entries_[index];
    }
  }

  // Insert moved elements.
  base::ranges::copy(moved_entries, entries_.begin() + insert_before);

  // Possibly large change, so tell the view to just rebuild itself.
  if (observer_)
    observer_->OnModelChanged();
}

void CustomHomePagesTableModel::AddWithoutNotification(size_t index,
                                                       const GURL& url) {
  DCHECK(index <= RowCount());
  entries_.insert(entries_.begin() + index, Entry());
  entries_[index].url = url;
}

void CustomHomePagesTableModel::Add(size_t index, const GURL& url) {
  AddWithoutNotification(index, url);
  LoadTitle(&(entries_[index]));
  if (observer_)
    observer_->OnItemsAdded(index, 1);
}

void CustomHomePagesTableModel::RemoveWithoutNotification(size_t index) {
  DCHECK(index < RowCount());
  Entry* entry = &(entries_[index]);
  // Cancel any pending load requests now so we don't deref a bogus pointer when
  // we get the loaded notification.
  if (entry->task_id != base::CancelableTaskTracker::kBadTaskId) {
    task_tracker_.TryCancel(entry->task_id);
    entry->task_id = base::CancelableTaskTracker::kBadTaskId;
  }
  entries_.erase(entries_.begin() + index);
}

void CustomHomePagesTableModel::Remove(size_t index) {
  RemoveWithoutNotification(index);
  if (observer_)
    observer_->OnItemsRemoved(index, 1);
}

void CustomHomePagesTableModel::SetToCurrentlyOpenPages(
    content::WebContents* ignore_contents) {
  // Remove the current entries.
  while (RowCount())
    RemoveWithoutNotification(0);

  // Add tabs from appropriate browser windows.
  size_t add_index = 0;
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (!ShouldIncludeBrowser(browser))
      continue;

    for (int tab_index = 0;
         tab_index < browser->tab_strip_model()->count();
         ++tab_index) {
      content::WebContents* contents =
          browser->tab_strip_model()->GetWebContentsAt(tab_index);
      if (contents == ignore_contents)
        continue;
      const GURL url = contents->GetURL();
      if (!url.is_empty() && !url.SchemeIs(content::kChromeDevToolsScheme))
        AddWithoutNotification(add_index++, url);
    }
  }
  LoadAllTitles();
}

std::vector<GURL> CustomHomePagesTableModel::GetURLs() {
  std::vector<GURL> urls(entries_.size());
  for (size_t i = 0; i < entries_.size(); ++i)
    urls[i] = entries_[i].url;
  return urls;
}

size_t CustomHomePagesTableModel::RowCount() {
  return entries_.size();
}

std::u16string CustomHomePagesTableModel::GetText(size_t row, int column_id) {
  DCHECK(column_id == 0);
  DCHECK(row < RowCount());
  return entries_[row].title.empty() ? FormattedURL(row) : entries_[row].title;
}

std::u16string CustomHomePagesTableModel::GetTooltip(size_t row) {
  return entries_[row].title.empty()
             ? std::u16string()
             : l10n_util::GetStringFUTF16(IDS_SETTINGS_ON_STARTUP_PAGE_TOOLTIP,
                                          entries_[row].title,
                                          FormattedURL(row));
}

void CustomHomePagesTableModel::SetObserver(ui::TableModelObserver* observer) {
  observer_ = observer;
}

bool CustomHomePagesTableModel::ShouldIncludeBrowser(Browser* browser) {
  // Do not include incognito browsers.
  if (browser->profile() != profile_)
    return false;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Do not include the Settings window.
  if (chrome::SettingsWindowManager::GetInstance()->IsSettingsBrowser(
          browser)) {
    return false;
  }
#endif
  return true;
}

void CustomHomePagesTableModel::LoadTitle(Entry* entry) {
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (history_service) {
    entry->task_id = history_service->QueryURL(
        entry->url, false,
        base::BindOnce(&CustomHomePagesTableModel::OnGotTitle,
                       base::Unretained(this), entry->url, false),
        &task_tracker_);
  }
}

void CustomHomePagesTableModel::LoadAllTitles() {
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  // It's possible for multiple LoadAllTitles() queries to be inflight we want
  // to make sure everything is resolved before updating the observer or we risk
  // getting rendering glitches.
  num_outstanding_title_lookups_ += entries_.size();
  for (Entry& entry : entries_) {
    if (history_service) {
      entry.task_id = history_service->QueryURL(
          entry.url, false,
          base::BindOnce(&CustomHomePagesTableModel::OnGotOneOfManyTitles,
                         base::Unretained(this), entry.url),
          &task_tracker_);
    }
  }
  if (entries_.empty())
    observer_->OnModelChanged();
}

void CustomHomePagesTableModel::OnGotOneOfManyTitles(
    const GURL& entry_url,
    history::QueryURLResult result) {
  OnGotTitle(entry_url, false, std::move(result));
  DCHECK_GE(num_outstanding_title_lookups_, 1);
  if (--num_outstanding_title_lookups_ == 0 && observer_)
    observer_->OnModelChanged();
}

void CustomHomePagesTableModel::OnGotTitle(const GURL& entry_url,
                                           bool observable,
                                           history::QueryURLResult result) {
  Entry* entry = nullptr;
  size_t entry_index = 0;
  for (size_t i = 0; i < entries_.size(); ++i) {
    if (entries_[i].url == entry_url) {
      entry = &entries_[i];
      entry_index = i;
      break;
    }
  }
  if (!entry) {
    // The URLs changed before we were called back.
    return;
  }
  entry->task_id = base::CancelableTaskTracker::kBadTaskId;
  if (result.success && !result.row.title().empty()) {
    entry->title = result.row.title();
    if (observer_ && observable)
      observer_->OnItemsChanged(entry_index, 1);
  }
}

std::u16string CustomHomePagesTableModel::FormattedURL(size_t row) const {
  std::u16string url = url_formatter::FormatUrl(entries_[row].url);
  url = base::i18n::GetDisplayStringInLTRDirectionality(url);
  return url;
}
