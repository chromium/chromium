// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"

#include "base/functional/callback_helpers.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kShouldShowTitleInSidePanelHeaderKey, true)

SidePanelEntry::SidePanelEntry(
    Key key,
    CreateContentCallback create_content_callback,
    base::RepeatingCallback<GURL()> open_in_new_tab_url_callback,
    base::RepeatingCallback<std::unique_ptr<ui::MenuModel>()>
        more_info_callback,
    base::RepeatingCallback<int()> default_content_width_callback)
    : type_(PanelType::kContent),
      key_(key),
      create_content_callback_(std::move(create_content_callback)),
      open_in_new_tab_url_callback_(std::move(open_in_new_tab_url_callback)),
      more_info_callback_(std::move(more_info_callback)),
      default_content_width_callback_(default_content_width_callback) {
  DCHECK(create_content_callback_);
}

SidePanelEntry::SidePanelEntry(
    PanelType type,
    Key key,
    CreateContentCallback create_content_callback,
    base::RepeatingCallback<int()> default_content_width_callback)
    : type_(type),
      key_(key),
      create_content_callback_(std::move(create_content_callback)),
      open_in_new_tab_url_callback_(base::NullCallback()),
      more_info_callback_(base::NullCallback()),
      default_content_width_callback_(default_content_width_callback) {
  DCHECK(create_content_callback_);
}

SidePanelEntry::SidePanelEntry(
    Key key,
    CreateContentCallback create_content_callback,
    base::RepeatingCallback<int()> default_content_width_callback)
    : SidePanelEntry(PanelType::kContent,
                     key,
                     std::move(create_content_callback),
                     default_content_width_callback) {}

SidePanelEntry::~SidePanelEntry() = default;

std::unique_ptr<views::View> SidePanelEntry::GetContent() {
  CHECK(scope_);
  if (content_view_) {
    return std::move(content_view_);
  }
  entry_show_triggered_timestamp_ = base::TimeTicks::Now();
  return create_content_callback_.Run(*scope_);
}

void SidePanelEntry::CacheView(std::unique_ptr<views::View> view) {
  content_view_ = std::move(view);
}

void SidePanelEntry::ClearCachedView() {
  content_view_.reset(nullptr);
}

void SidePanelEntry::OnEntryShown() {
  entry_shown_timestamp_ = base::TimeTicks::Now();
  SidePanelUtil::RecordEntryShownMetrics(type(), key_.id(),
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

void SidePanelEntry::OnEntryHideCancelled() {
  observers_.Notify(&SidePanelEntryObserver::OnEntryHideCancelled, this);
}

void SidePanelEntry::OnEntryHidden() {
  SidePanelUtil::RecordEntryHiddenMetrics(type(), key_.id(),
                                          entry_shown_timestamp_);
  observers_.Notify(&SidePanelEntryObserver::OnEntryHidden, this);
}

void SidePanelEntry::AddObserver(SidePanelEntryObserver* observer) {
  observers_.AddObserver(observer);
}

void SidePanelEntry::RemoveObserver(SidePanelEntryObserver* observer) {
  observers_.RemoveObserver(observer);
}

GURL SidePanelEntry::GetOpenInNewTabURL() const {
  if (open_in_new_tab_url_callback_.is_null()) {
    return GURL();
  }

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

int SidePanelEntry::GetDefaultContentWidth() const {
  if (default_content_width_callback_.is_null()) {
    return default_content_width_;
  }

  int preferred_default_width = default_content_width_callback_.Run();
  // The default width must be greater than or equal to the default side panel
  // width.
  return preferred_default_width >= kSidePanelDefaultContentWidth
             ? preferred_default_width
             : default_content_width_;
}
