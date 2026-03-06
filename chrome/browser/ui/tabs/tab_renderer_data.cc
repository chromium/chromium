// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_renderer_data.h"

#include "base/byte_size.h"
#include "base/process/kill.h"
#include "build/build_config.h"
#include "chrome/browser/collaboration/messaging/messaging_backend_service_factory.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-shared.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/performance_controls/memory_saver_utils.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_tab_data.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "chrome/browser/ui/thumbnails/thumbnail_tab_helper.h"
#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_tabbed_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "components/collaboration/public/messaging/messaging_backend_service.h"
#include "components/performance_manager/public/features.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"

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

// static
TabRendererData TabRendererData::FromTabInterface(tabs::TabInterface* tab) {
  CHECK(tab);
  content::WebContents* const contents = tab->GetContents();
  CHECK(contents);

  TabRendererData data;

  TabUIHelper* const tab_ui_helper = TabUIHelper::From(tab);
  data.favicon = tab_ui_helper->GetFavicon();
  data.title = tab_ui_helper->GetTitle();
  data.needs_attention = tab_ui_helper->needs_attention();
  data.is_monochrome_favicon = tab_ui_helper->IsMonochromeFavicon();

  ThumbnailTabHelper* const thumbnail_tab_helper =
      ThumbnailTabHelper::FromWebContents(contents);
  if (thumbnail_tab_helper) {
    data.thumbnail = thumbnail_tab_helper->thumbnail();
  }
  data.is_tab_discarded = contents->WasDiscarded();

  data.collaboration_messaging = GetCollaborationMessage(tab);
  data.network_state = TabNetworkStateForWebContents(contents);

  data.visible_url = tab_ui_helper->GetVisibleURL();
  data.should_render_loading_title = tab_ui_helper->ShouldRenderLoadingTitle();
  data.last_committed_url = contents->GetLastCommittedURL();
  data.should_display_url = tab_ui_helper->ShouldDisplayURL();
  data.is_crashed = tab_ui_helper->IsCrashed();
  data.pinned = tab->IsPinned();
  data.show_icon = tab_ui_helper->ShouldDisplayFavicon();
  data.blocked = tab->IsBlocked();
  data.should_hide_throbber = tab_ui_helper->ShouldHideThrobber();
  data.alert_state = tabs::TabAlertController::From(tab)->GetAllActiveAlerts();
  data.should_themify_favicon = tab_ui_helper->ShouldThemifyFavicon();

  data.should_show_discard_status = tab_ui_helper->ShouldShowDiscardStatus();
  data.discarded_memory_savings =
      tab_ui_helper->GetDiscardedMemorySavings().value_or(base::ByteSize());
  if (const auto* const resource_tab_helper =
          TabResourceUsageTabHelper::From(tab)) {
    data.tab_resource_usage = resource_tab_helper->resource_usage();
  }

  return data;
}

TabRendererData::TabRendererData() = default;
TabRendererData::TabRendererData(const TabRendererData& other) = default;
TabRendererData::TabRendererData(TabRendererData&& other) = default;

TabRendererData& TabRendererData::operator=(const TabRendererData& other) =
    default;
TabRendererData& TabRendererData::operator=(TabRendererData&& other) = default;

TabRendererData::~TabRendererData() = default;

bool TabRendererData::operator==(const TabRendererData& other) const {
  return favicon == other.favicon && thumbnail == other.thumbnail &&
         network_state == other.network_state && title == other.title &&
         visible_url == other.visible_url &&
         last_committed_url == other.last_committed_url &&
         should_display_url == other.should_display_url &&
         is_crashed == other.is_crashed && show_icon == other.show_icon &&
         pinned == other.pinned && blocked == other.blocked &&
         alert_state == other.alert_state &&
         should_hide_throbber == other.should_hide_throbber &&
         is_tab_discarded == other.is_tab_discarded &&
         should_show_discard_status == other.should_show_discard_status &&
         discarded_memory_savings == other.discarded_memory_savings &&
         tab_resource_usage == other.tab_resource_usage &&
         is_monochrome_favicon == other.is_monochrome_favicon &&
         needs_attention == other.needs_attention;
}
