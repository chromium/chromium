// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model_stats_recorder.h"

#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/supports_user_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

TabStripModelStatsRecorder::TabStripModelStatsRecorder()
    : browser_tab_strip_tracker_(
          std::make_unique<BrowserTabStripTracker>(this, nullptr)) {
  browser_tab_strip_tracker_->Init();
}

TabStripModelStatsRecorder::~TabStripModelStatsRecorder() {
}

class TabStripModelStatsRecorder::TabInfo
    : public base::SupportsUserData::Data {
 public:
  ~TabInfo() override;
  void UpdateState(TabState new_state);

  TabState state() const { return current_state_; }

  static TabInfo* Get(content::WebContents* contents) {
    TabInfo* info = static_cast<TabStripModelStatsRecorder::TabInfo*>(
        contents->GetUserData(kKey));
    if (!info) {
      info = new TabInfo();
      contents->SetUserData(kKey, base::WrapUnique(info));
    }
    return info;
  }

 private:
  TabState current_state_ = TabState::INITIAL;

  static const char kKey[];
};

const char TabStripModelStatsRecorder::TabInfo::kKey[] = "WebContents TabInfo";

TabStripModelStatsRecorder::TabInfo::~TabInfo() {}

void TabStripModelStatsRecorder::TabInfo::UpdateState(TabState new_state) {
  if (new_state == current_state_)
    return;

  // Avoid state transition from CLOSED.
  // When tab is closed, we receive TabStripModelObserver::TabClosingAt and then
  // TabStripModelStatsRecorder::ActiveTabChanged.
  // Here we ignore CLOSED -> INACTIVE state transition from last
  // ActiveTabChanged.
  if (current_state_ == TabState::CLOSED)
    return;

  switch (current_state_) {
    case TabState::INITIAL:
    case TabState::ACTIVE:
    case TabState::INACTIVE:
      break;
    case TabState::CLOSED:
    case TabState::MAX:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  current_state_ = new_state;
}

void TabStripModelStatsRecorder::OnTabClosing(content::WebContents* contents) {
  TabInfo::Get(contents)->UpdateState(TabState::CLOSED);

  // Avoid having stale pointer in active_tab_history_
  std::replace(active_tab_history_.begin(), active_tab_history_.end(), contents,
               static_cast<content::WebContents*>(nullptr));
}

void TabStripModelStatsRecorder::OnActiveTabChanged(
    content::WebContents* old_contents,
    content::WebContents* new_contents,
    int reason) {
  if (reason & TabStripModelObserver::CHANGE_REASON_REPLACED) {
    // We already handled tab clobber at TabReplacedAt notification.
    return;
  }

  if (old_contents)
    TabInfo::Get(old_contents)->UpdateState(TabState::INACTIVE);

  DCHECK(new_contents);
  TabInfo* tab_info = TabInfo::Get(new_contents);
  tab_info->UpdateState(TabState::ACTIVE);

  // A UMA Histogram must be bounded by some number.
  // We chose 64 as our bound as 99.5% of the users open <64 tabs.
  const int kMaxTabHistory = 64;
  active_tab_history_.insert(active_tab_history_.begin(), new_contents);
  if (active_tab_history_.size() > kMaxTabHistory)
    active_tab_history_.resize(kMaxTabHistory);
}

void TabStripModelStatsRecorder::OnTabReplaced(
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  DCHECK(old_contents != new_contents);
  *TabInfo::Get(new_contents) = *TabInfo::Get(old_contents);

  std::replace(active_tab_history_.begin(), active_tab_history_.end(),
               old_contents, new_contents);
}

void TabStripModelStatsRecorder::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kRemoved) {
    for (const auto& contents : change.GetRemove()->contents) {
      if (contents.remove_reason == TabStripModelChange::RemoveReason::kDeleted)
        OnTabClosing(contents.contents);
    }
  } else if (change.type() == TabStripModelChange::kReplaced) {
    auto* replace = change.GetReplace();
    OnTabReplaced(replace->old_contents, replace->new_contents);
  }

  if (!selection.active_tab_changed() || tab_strip_model->empty())
    return;

  OnActiveTabChanged(selection.old_contents, selection.new_contents,
                     selection.reason);
}
