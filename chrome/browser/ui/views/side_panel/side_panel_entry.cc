// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"

#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kShouldShowTitleInSidePanelHeaderKey, true)

SidePanelEntry::SidePanelEntry(
    Id id,
    base::RepeatingCallback<std::unique_ptr<views::View>()>
        create_content_callback,
    std::optional<base::RepeatingCallback<GURL()>> open_in_new_tab_url_callback,
    std::optional<base::RepeatingCallback<std::unique_ptr<ui::MenuModel>()>>
        more_info_callback)
    : key_(id), create_content_callback_(std::move(create_content_callback)) {
  open_in_new_tab_url_callback_ =
      open_in_new_tab_url_callback.value_or(base::NullCallbackAs<GURL()>());
  more_info_callback_ = more_info_callback.value_or(
      base::NullCallbackAs<std::unique_ptr<ui::MenuModel>()>());
}

SidePanelEntry::SidePanelEntry(
    Key key,
    base::RepeatingCallback<std::unique_ptr<views::View>()>
        create_content_callback)
    : key_(key),
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

void SidePanelEntry::OnEntryShown() {
  entry_shown_timestamp_ = base::TimeTicks::Now();
  SidePanelUtil::RecordEntryShownMetrics(key_.id(),
                                         entry_show_triggered_timestamp_);
  // After the initial load time is recorded, we need to reset the triggered
  // timestamp so we don't keep recording this entry after its selected from the
  // combobox.
  ResetLoadTimestamp();
  observers_.Notify(&SidePanelEntryObserver::OnEntryShown, this);
}

void SidePanelEntry::OnEntryWillHide(SidePanelEntryHideReason reason) {
  observers_.Notify(&SidePanelEntryObserver::OnEntryWillHide, this, reason);
}

void SidePanelEntry::OnEntryHidden() {
  SidePanelUtil::RecordEntryHiddenMetrics(key_.id(), entry_shown_timestamp_);
  observers_.Notify(&SidePanelEntryObserver::OnEntryHidden, this);
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

std::unique_ptr<ui::MenuModel> SidePanelEntry::GetMoreInfoMenuModel() const {
  if (more_info_callback_.is_null()) {
    return nullptr;
  }

  return more_info_callback_.Run();
}

bool SidePanelEntry::SupportsNewTabButton() {
  return !open_in_new_tab_url_callback_.is_null();
}

bool SidePanelEntry::SupportsMoreInfoButton() {
  return !more_info_callback_.is_null();
}

void SidePanelEntry::ResetLoadTimestamp() {
  entry_show_triggered_timestamp_ = base::TimeTicks();
}
