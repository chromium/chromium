// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"

#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"

SidePanelEntry::SidePanelEntry(
    Id id,
    std::u16string name,
    ui::ImageModel icon,
    base::RepeatingCallback<std::unique_ptr<views::View>()>
        create_content_callback,
    base::RepeatingCallback<GURL()> open_in_new_tab_url_callback)
    : key_(id),
      name_(std::move(name)),
      icon_(std::move(icon)),
      create_content_callback_(std::move(create_content_callback)),
      open_in_new_tab_url_callback_(std::move(open_in_new_tab_url_callback)) {}

SidePanelEntry::SidePanelEntry(
    Key key,
    std::u16string name,
    ui::ImageModel icon,
    base::RepeatingCallback<std::unique_ptr<views::View>()>
        create_content_callback)
    : key_(key),
      name_(std::move(name)),
      icon_(std::move(icon)),
      create_content_callback_(std::move(create_content_callback)) {
  DCHECK(create_content_callback_);
}

SidePanelEntry::~SidePanelEntry() = default;

std::unique_ptr<views::View> SidePanelEntry::GetContent() {
  if (content_view_)
    return std::move(content_view_);
  entry_show_triggered_timestamp_ = base::TimeTicks::Now();
  return create_content_callback_.Run();
}

void SidePanelEntry::CacheView(std::unique_ptr<views::View> view) {
  content_view_ = std::move(view);
}

void SidePanelEntry::ClearCachedView() {
  content_view_.reset(nullptr);
}

void SidePanelEntry::ResetIcon(ui::ImageModel icon) {
  icon_ = std::move(icon);
  for (SidePanelEntryObserver& observer : observers_)
    observer.OnEntryIconUpdated(this);
}

void SidePanelEntry::OnEntryShown() {
  entry_shown_timestamp_ = base::TimeTicks::Now();
  SidePanelUtil::RecordEntryShownMetrics(key_.id(),
                                         entry_show_triggered_timestamp_);
  for (SidePanelEntryObserver& observer : observers_)
    observer.OnEntryShown(this);
}

void SidePanelEntry::OnEntryHidden() {
  SidePanelUtil::RecordEntryHiddenMetrics(key_.id(), entry_shown_timestamp_);
  for (SidePanelEntryObserver& observer : observers_)
    observer.OnEntryHidden(this);
}

void SidePanelEntry::AddObserver(SidePanelEntryObserver* observer) {
  observers_.AddObserver(observer);
}

void SidePanelEntry::RemoveObserver(SidePanelEntryObserver* observer) {
  observers_.RemoveObserver(observer);
}

GURL SidePanelEntry::GetOpenInNewTabURL() const {
  if (open_in_new_tab_url_callback_.is_null())
    return GURL();

  return open_in_new_tab_url_callback_.Run();
}

bool SidePanelEntry::SupportsNewTabButton() {
  return !open_in_new_tab_url_callback_.is_null();
}

void SidePanelEntry::ResetLoadTimestamp() {
  entry_show_triggered_timestamp_ = base::TimeTicks();
}
