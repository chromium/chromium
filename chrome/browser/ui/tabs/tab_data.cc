// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_data.h"

#include <string>

#include "base/functional/bind.h"
#include "chrome/browser/collaboration/messaging/messaging_backend_service_factory.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_tab_data.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "chrome/browser/ui/thumbnails/thumbnail_tab_helper.h"
#include "components/collaboration/public/messaging/messaging_backend_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace {
using collaboration::messaging::MessagingBackendService;
using collaboration::messaging::MessagingBackendServiceFactory;
using collaboration::messaging::PersistentMessage;

base::WeakPtr<tab_groups::CollaborationMessagingTabData>
GetCollaborationMessage(tabs::TabInterface* tab) {
  if (!tab_groups::SavedTabGroupUtils::SupportsSharedTabGroups()) {
    return nullptr;
  }

  auto* data = tab->GetTabFeatures()->collaboration_messaging_tab_data();
  if (!data) {
    return nullptr;
  }

  return data->GetWeakPtr();
}
}  // namespace

namespace tabs {

TabData TabData::FromTabInterface(tabs::TabInterface* tab_interface) {
  TabUIHelper* const tab_ui_helper = TabUIHelper::From(tab_interface);
  tabs::TabAlertController* const tab_alert_controller =
      tabs::TabAlertController::From(tab_interface);

  TabData tab_data;
  tab_data.title = tab_ui_helper->GetTitle();
  tab_data.should_render_loading_title =
      tab_ui_helper->ShouldRenderLoadingTitle();
  tab_data.should_themify_favicon = tab_ui_helper->ShouldThemifyFavicon();
  tab_data.should_display_favicon =
      tab_interface->IsPinned() || tab_ui_helper->ShouldDisplayFavicon();
  tab_data.is_monochrome_favicon = tab_ui_helper->IsMonochromeFavicon();
  tab_data.favicon = tab_ui_helper->GetFavicon();
  tab_data.should_hide_throbber = tab_ui_helper->ShouldHideThrobber();
  tab_data.is_crashed = tab_ui_helper->IsCrashed();
  tab_data.should_display_url = tab_ui_helper->ShouldDisplayURL();
  tab_data.visible_url = tab_ui_helper->GetVisibleURL();
  tab_data.needs_attention = tab_ui_helper->needs_attention();
  tab_data.should_show_discard_status =
      tab_ui_helper->ShouldShowDiscardStatus();
  tab_data.discarded_memory_savings =
      tab_ui_helper->GetDiscardedMemorySavings();
  tab_data.network_state = tab_ui_helper->GetTabNetworkState();
  tab_data.is_tab_discarded = tab_ui_helper->IsDiscarded();
  tab_data.last_committed_url = tab_ui_helper->GetLastCommittedURL();
  tab_data.alert_state = tab_alert_controller->GetAlertToShow();
  tab_data.pinned = tab_interface->IsPinned();
  tab_data.blocked = tab_interface->IsBlocked();

  content::WebContents* const web_contents = tab_interface->GetContents();
  ThumbnailTabHelper* const thumbnail_tab_helper =
      ThumbnailTabHelper::FromWebContents(web_contents);
  tab_data.thumbnail =
      thumbnail_tab_helper ? thumbnail_tab_helper->thumbnail() : nullptr;

  TabResourceUsageTabHelper* const tab_resource_usage_tab_helper =
      TabResourceUsageTabHelper::From(tab_interface);
  tab_data.tab_resource_usage = tab_resource_usage_tab_helper->resource_usage();

  tab_data.collaboration_messaging = GetCollaborationMessage(tab_interface);

  return tab_data;
}

TabData::TabData() = default;
TabData::TabData(const TabData& other) = default;
TabData::TabData(TabData&& other) = default;
TabData::~TabData() = default;
TabData& TabData::operator=(const TabData& other) = default;
TabData& TabData::operator=(TabData&& other) = default;

bool TabData::operator==(const TabData& other) const {
  return title == other.title &&
         should_render_loading_title == other.should_render_loading_title &&
         should_themify_favicon == other.should_themify_favicon &&
         should_display_favicon == other.should_display_favicon &&
         is_monochrome_favicon == other.is_monochrome_favicon &&
         favicon == other.favicon &&
         should_hide_throbber == other.should_hide_throbber &&
         is_crashed == other.is_crashed &&
         should_display_url == other.should_display_url &&
         visible_url == other.visible_url &&
         needs_attention == other.needs_attention &&
         should_show_discard_status == other.should_show_discard_status &&
         discarded_memory_savings == other.discarded_memory_savings &&
         network_state == other.network_state &&
         is_tab_discarded == other.is_tab_discarded &&
         last_committed_url == other.last_committed_url &&
         alert_state == other.alert_state && pinned == other.pinned &&
         blocked == other.blocked && thumbnail == other.thumbnail &&
         tab_resource_usage == other.tab_resource_usage;
}

TabDataObserver::TabDataObserver(TabInterface* tab_interface)
    : tab_interface_(tab_interface) {
  tab_ui_change_subscription_ =
      TabUIHelper::From(tab_interface_)
          ->AddTabUIChangeCallback(base::BindRepeating(
              &TabDataObserver::OnTabUIChange, base::Unretained(this)));
  alert_change_subscription_ =
      tabs::TabAlertController::From(tab_interface_)
          ->AddAlertToShowChangedCallback(base::BindRepeating(
              &TabDataObserver::OnAlertsChanged, base::Unretained(this)));
  pinned_state_change_subscription_ =
      tab_interface_->RegisterPinnedStateChanged(base::BindRepeating(
          &TabDataObserver::OnPinnedStateChanged, base::Unretained(this)));
  blocked_state_change_subscription_ =
      tab_interface_->RegisterBlockedStateChanged(base::BindRepeating(
          &TabDataObserver::OnBlockedStateChanged, base::Unretained(this)));
  tab_detached_subscription_ =
      tab_interface_->RegisterWillDetach(base::BindRepeating(
          &TabDataObserver::OnTabDetached, base::Unretained(this)));

  OnTabUIChange();
  OnAlertsChanged(
      tabs::TabAlertController::From(tab_interface_)->GetAlertToShow());
  OnBlockedStateChanged(tab_interface_, tab_interface_->IsBlocked());
  OnPinnedStateChanged(tab_interface_, tab_interface_->IsPinned());
}

TabDataObserver::~TabDataObserver() = default;

base::CallbackListSubscription TabDataObserver::RegisterTabDataChangedCallback(
    base::RepeatingCallback<void(TabChangeType, const TabData&)> callback) {
  return tab_data_changed_callback_list_.Add(std::move(callback));
}

void TabDataObserver::NotifyTabDataChanged(TabChangeType change_type) {
  tab_data_changed_callback_list_.Notify(change_type, tab_data_);
}

void TabDataObserver::OnTabUIChange() {
  TabData updated_tab_data = TabData::FromTabInterface(tab_interface_);
  if (tab_data_ != updated_tab_data) {
    const TabChangeType change_type =
        tab_data_.needs_attention != updated_tab_data.needs_attention
            ? TabChangeType::kAttentionOnly
            : TabChangeType::kAll;
    tab_data_ = updated_tab_data;
    NotifyTabDataChanged(change_type);
  }
}

void TabDataObserver::OnAlertsChanged(std::optional<TabAlert> alert_to_show) {
  if (tab_data_.alert_state != alert_to_show) {
    tab_data_.alert_state = alert_to_show;
    NotifyTabDataChanged(TabChangeType::kAll);
  }
}

void TabDataObserver::OnPinnedStateChanged(tabs::TabInterface* tab_interface,
                                           bool new_pinned_state) {
  if (tab_data_.pinned != new_pinned_state) {
    tab_data_.pinned = new_pinned_state;
    NotifyTabDataChanged(TabChangeType::kAll);
  }
}

void TabDataObserver::OnBlockedStateChanged(tabs::TabInterface* tab_interface,
                                            bool new_blocked_state) {
  if (tab_data_.blocked != new_blocked_state) {
    tab_data_.blocked = new_blocked_state;
    NotifyTabDataChanged(TabChangeType::kBlockedOnly);
  }
}

void TabDataObserver::OnTabDetached(tabs::TabInterface* tab_interface,
                                    tabs::TabInterface::DetachReason reason) {
  if (reason == tabs::TabInterface::DetachReason::kDelete) {
    tab_ui_change_subscription_ = base::CallbackListSubscription();
    alert_change_subscription_ = base::CallbackListSubscription();
    pinned_state_change_subscription_ = base::CallbackListSubscription();
    blocked_state_change_subscription_ = base::CallbackListSubscription();
    tab_detached_subscription_ = base::CallbackListSubscription();
    tab_interface_ = nullptr;
  }
}

}  // namespace tabs
