// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab/tab_accessibility.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_tab_data.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/permissions/chip/chip_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

namespace {
int GetAccessibleTabLabelFormatStringForSplit(split_tabs::SplitTabLayout layout,
                                              int tab_index_in_split) {
  switch (layout) {
    case split_tabs::SplitTabLayout::kVertical:
      switch (tab_index_in_split) {
        case 0:
          return IDS_TAB_AX_LABEL_SPLIT_TAB_LEFT_VIEW_FORMAT;
        case 1:
          return IDS_TAB_AX_LABEL_SPLIT_TAB_RIGHT_VIEW_FORMAT;
        default:
          NOTREACHED();
      }
    default:
      NOTREACHED();
  }
}
}  // namespace

namespace tabs {

bool ShouldUpdateAccessibleName(const TabRendererData& old_data,
                                const TabRendererData& new_data) {
  bool has_old_message = old_data.collaboration_messaging &&
                         old_data.collaboration_messaging->HasMessage();
  bool has_new_message = new_data.collaboration_messaging &&
                         new_data.collaboration_messaging->HasMessage();
  bool collaboration_message_changed = has_old_message != has_new_message;
  if (!collaboration_message_changed && has_old_message) {
    // Old and new data have both have messages, so compare the contents.
    collaboration_message_changed =
        (old_data.collaboration_messaging->given_name() !=
         new_data.collaboration_messaging->given_name()) ||
        (old_data.collaboration_messaging->collaboration_event() !=
         new_data.collaboration_messaging->collaboration_event());
  }

  return ((old_data.network_state != new_data.network_state) ||
          old_data.is_crashed != new_data.is_crashed ||
          old_data.alert_state != new_data.alert_state ||
          old_data.should_show_discard_status !=
              new_data.should_show_discard_status ||
          old_data.discarded_memory_savings !=
              new_data.discarded_memory_savings ||
          old_data.tab_resource_usage != new_data.tab_resource_usage ||
          old_data.pinned != new_data.pinned ||
          old_data.title != new_data.title || collaboration_message_changed);
}

std::u16string GetAccessibleTabLabel(const TabInterface* tab, bool is_for_tab) {
  const BrowserWindowInterface* bwi = tab->GetBrowserWindowInterface();
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(bwi);
  Browser* browser = browser_view->browser();
  TabStripModel* tab_strip_model = browser->tab_strip_model();
  int index = tab_strip_model->GetIndexOfTab(tab);
  if (index == TabStripModel::kNoTab) {
    return std::u16string();
  }

  const TabRendererData& tab_data =
      browser_view->tab_strip_view()->GetTabRendererData(index);

  std::u16string title = is_for_tab ? browser->GetTitleForTab(index)
                                    : browser->GetWindowTitleForTab(index);

  if (const std::optional<split_tabs::SplitTabId> split = tab->GetSplit()) {
    const split_tabs::SplitTabData* split_data =
        tab_strip_model->GetSplitData(split.value());
    const std::vector<tabs::TabInterface*> tabs_in_split =
        split_data->ListTabs();

    int tab_index_in_split = std::distance(
        tabs_in_split.begin(),
        std::find(tabs_in_split.begin(), tabs_in_split.end(), tab));
    title = l10n_util::GetStringFUTF16(
        GetAccessibleTabLabelFormatStringForSplit(
            split_tabs::SplitTabLayout::kVertical, tab_index_in_split),
        title);
  }

  if (const std::optional<tab_groups::TabGroupId> group = tab->GetGroup()) {
    std::u16string group_title = tab_strip_model->group_model()
                                     ->GetTabGroup(group.value())
                                     ->visual_data()
                                     ->title();
    if (group_title.empty()) {
      title = l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_UNNAMED_GROUP_FORMAT,
                                         title);
    } else {
      title = l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_NAMED_GROUP_FORMAT,
                                         title, group_title);
    }
  }

  // Tab is pinned.
  if (tab->IsPinned()) {
    title = l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_PINNED_FORMAT, title);
  }

  // Tab has crashed.
  if (tab_data.is_crashed) {
    return l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_CRASHED_FORMAT, title);
  }

  // Network error interstitial.
  if (tab_data.network_state == TabNetworkState::kError) {
    return l10n_util::GetStringFUTF16(IDS_TAB_AX_LABEL_NETWORK_ERROR_FORMAT,
                                      title);
  }

  // Tab has a pending permission request.
  LocationBar* location_bar = browser_view->GetLocationBar();
  if (location_bar && location_bar->GetChipController() &&
      location_bar->GetChipController()->IsPermissionPromptChipVisible()) {
    return l10n_util::GetStringFUTF16(
        IDS_TAB_AX_LABEL_PERMISSION_REQUESTED_FORMAT, title);
  }

  // Alert tab states.
  const tabs::TabAlertController* tab_alert_controller =
      tabs::TabAlertController::From(tab);

  // During tab destruction, the alert controller may already be destroyed
  // by the time accessibility name updates are processed.
  if (tab_alert_controller) {
    if (const std::optional<tabs::TabAlert> alert =
            tab_alert_controller->GetAlertToShow()) {
      title = l10n_util::GetStringFUTF16(
          tabs::TabAlertController::GetAccessibleAlertStringId(alert.value()),
          title);
    }
  }

  if (tab_data.should_show_discard_status) {
    title = l10n_util::GetStringFUTF16(IDS_TAB_AX_INACTIVE_TAB, title);
    if (tab_data.discarded_memory_savings.is_positive()) {
      title = l10n_util::GetStringFUTF16(
          IDS_TAB_AX_MEMORY_SAVINGS, title,
          ui::FormatBytes(tab_data.discarded_memory_savings));
    }
  } else if (tab_data.tab_resource_usage &&
             tab_data.tab_resource_usage->memory_usage().is_positive()) {
    const base::ByteSize memory_used =
        tab_data.tab_resource_usage->memory_usage();
    const bool is_high_memory_usage =
        tab_data.tab_resource_usage->is_high_memory_usage();
    if (is_high_memory_usage || is_for_tab) {
      const int message_id = is_high_memory_usage ? IDS_TAB_AX_HIGH_MEMORY_USAGE
                                                  : IDS_TAB_AX_MEMORY_USAGE;
      title = l10n_util::GetStringFUTF16(message_id, title,
                                         ui::FormatBytes(memory_used));
    }
  } else if (tab_data.collaboration_messaging &&
             tab_data.collaboration_messaging->HasMessage()) {
    std::u16string given_name = tab_data.collaboration_messaging->given_name();

    switch (tab_data.collaboration_messaging->collaboration_event()) {
      case collaboration::messaging::CollaborationEvent::TAB_ADDED:
        title = l10n_util::GetStringFUTF16(
                    IDS_DATA_SHARING_RECENT_ACTIVITY_MEMBER_ADDED_THIS_TAB,
                    given_name) +
                u", " + title;
        break;
      case collaboration::messaging::CollaborationEvent::TAB_UPDATED:
        title = l10n_util::GetStringFUTF16(
                    IDS_DATA_SHARING_RECENT_ACTIVITY_MEMBER_CHANGED_THIS_TAB,
                    given_name) +
                u", " + title;
        break;
      default:
        NOTREACHED();
    }
  }

  return title;
}

}  // namespace tabs
