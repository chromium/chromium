// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_focus_announcer.h"

#include <algorithm>
#include <utility>

#include "base/check_op.h"
#include "chrome/browser/ui/browser_init_state.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/groups/tab_group_accessibility.h"
#include "chrome/grit/generated_resources.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

std::u16string FormatEnteredAnnouncement(const TabGroup& group) {
  if (group.is_ephemeral()) {
    return l10n_util::GetPluralStringFUTF16(
        IDS_TAB_GROUP_AX_ANNOUNCE_FOCUSED_EPHEMERAL_GROUP, group.tab_count());
  }

  const std::u16string& title = group.visual_data()->title();
  if (!title.empty()) {
    return l10n_util::GetStringFUTF16(
        IDS_TAB_GROUP_AX_ANNOUNCE_FOCUSED_NAMED_GROUP, title);
  }

  CHECK_GT(group.tab_count(), 0);
  return l10n_util::GetStringFUTF16(
      IDS_TAB_GROUP_AX_ANNOUNCE_FOCUSED_UNNAMED_GROUP,
      tab_groups::GetGroupContentString(&group));
}

std::u16string FormatExitedAnnouncement(const TabGroup& group) {
  if (group.is_ephemeral()) {
    // Closing the last tab drops `tab_count()` to 0; clamp to 1 for singular.
    return l10n_util::GetPluralStringFUTF16(
        IDS_NON_GROUP_FOCUS_EXITED_TOAST_BODY, std::max(1, group.tab_count()));
  }
  return l10n_util::GetStringUTF16(IDS_TAB_GROUP_FOCUS_EXITED_TOAST_BODY);
}

}  // namespace

DEFINE_USER_DATA(TabStripFocusAnnouncer);

TabStripFocusAnnouncer::TabStripFocusAnnouncer(BrowserWindowInterface* browser)
    : scoped_data_(browser->GetUnownedUserDataHost(), *this) {
  if (const auto* init_state = BrowserInitState::From(browser)) {
    is_restoring_focused_group_ =
        init_state->initial_focused_tab_group_id().has_value();
  }
  if (auto* const elements = BrowserElementsViews::From(browser)) {
    if (views::Widget* const widget = elements->GetPrimaryWindowWidget()) {
      widget_observation_.Observe(widget);
    }
  }
}

TabStripFocusAnnouncer::~TabStripFocusAnnouncer() = default;

// static
TabStripFocusAnnouncer* TabStripFocusAnnouncer::From(
    BrowserWindowInterface* browser) {
  return browser ? ui::ScopedUnownedUserData<TabStripFocusAnnouncer>::Get(
                       browser->GetUnownedUserDataHost())
                 : nullptr;
}

void TabStripFocusAnnouncer::AnnounceFocusEntered(const TabGroup& group) {
  if (std::exchange(is_restoring_focused_group_, false)) {
    return;
  }
  Announce(FormatEnteredAnnouncement(group));
}

void TabStripFocusAnnouncer::AnnounceFocusExited(const TabGroup& group) {
  Announce(FormatExitedAnnouncement(group));
}

void TabStripFocusAnnouncer::CancelPendingAnnouncement() {
  pending_announcement_.clear();
}

void TabStripFocusAnnouncer::OnWidgetActivationChanged(views::Widget* widget,
                                                       bool active) {
  if (!active) {
    CancelPendingAnnouncement();
    return;
  }
  if (!pending_announcement_.empty()) {
    FlushAnnouncement(widget);
  }
}

void TabStripFocusAnnouncer::OnWidgetDestroying(views::Widget* widget) {
  widget_observation_.Reset();
  CancelPendingAnnouncement();
}

void TabStripFocusAnnouncer::Announce(std::u16string announcement) {
  views::Widget* const widget = widget_observation_.GetSource();
  if (!widget || !widget->IsVisible()) {
    return;
  }

  if (widget->IsActive()) {
    pending_announcement_ = std::move(announcement);
    FlushAnnouncement(widget);
  } else if (widget->ShouldPaintAsActive()) {
    // A child bubble (such as the tab group editor bubble) currently owns
    // activation. Defer until activation returns to the browser window, since
    // macOS VoiceOver drops announcements posted on inactive windows.
    pending_announcement_ = std::move(announcement);
  } else {
    CancelPendingAnnouncement();
  }
}

void TabStripFocusAnnouncer::FlushAnnouncement(views::Widget* widget) {
  std::u16string announcement = std::exchange(pending_announcement_, {});
  if (announcement.empty() || !widget || !widget->IsVisible() ||
      !widget->IsActive()) {
    return;
  }

  widget->GetRootView()->GetViewAccessibility().AnnounceAlert(announcement);
}
