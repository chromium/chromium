// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_DATA_H_
#define CHROME_BROWSER_UI_TABS_TAB_DATA_H_

#include <string>

#include "base/byte_size.h"
#include "base/callback_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_network_state.h"
#include "ui/base/models/image_model.h"
#include "url/gurl.h"

class TabResourceUsage;
class ThumbnailImage;

namespace tab_groups {
class CollaborationMessagingTabData;
}  // namespace tab_groups

namespace tabs {
class TabInterface;

struct TabData {
  static TabData FromTabInterface(tabs::TabInterface* tab);

  TabData();
  TabData(const TabData& other);
  TabData(TabData&& other);
  ~TabData();
  TabData& operator=(const TabData& other);
  TabData& operator=(TabData&& other);

  bool operator==(const TabData& other) const;

  // TabUIHelper properties:
  std::u16string title;
  bool should_render_loading_title = false;
  bool should_themify_favicon = false;
  bool should_display_favicon = true;
  bool is_monochrome_favicon = false;
  ui::ImageModel favicon;
  bool should_hide_throbber = false;
  bool is_crashed = false;
  bool should_display_url = true;
  GURL visible_url;
  bool needs_attention = false;
  bool should_show_discard_status = false;
  std::optional<base::ByteSize> discarded_memory_savings;
  TabNetworkState network_state = TabNetworkState::kNone;
  bool is_tab_discarded = false;
  GURL last_committed_url;

  // Alert properties:
  std::optional<TabAlert> alert_state;

  // TabInterface properties:
  bool pinned = false;
  bool blocked = false;

  scoped_refptr<ThumbnailImage> thumbnail;
  scoped_refptr<const TabResourceUsage> tab_resource_usage;
  base::WeakPtr<tab_groups::CollaborationMessagingTabData>
      collaboration_messaging = nullptr;
};

// Caches various data about a tab and notifies subscribers when any of the
// data property updates.
class TabDataObserver {
 public:
  explicit TabDataObserver(tabs::TabInterface* tab_interface);
  ~TabDataObserver();

  base::CallbackListSubscription RegisterTabDataChangedCallback(
      base::RepeatingCallback<void(TabChangeType, const TabData&)> callback);

  const TabData& tab_data() { return tab_data_; }

 private:
  void MaybeUpdateShouldThemifyFavicon();
  void NotifyTabDataChanged(TabChangeType change_type);
  void OnTabUIChange();
  void OnAlertsChanged(std::optional<TabAlert> alert_to_show);
  void OnPinnedStateChanged(tabs::TabInterface* tab_interface,
                            bool new_pinned_state);
  void OnBlockedStateChanged(tabs::TabInterface* tab_interface,
                             bool new_blocked_state);
  void OnTabDetached(tabs::TabInterface* tab_interface,
                     tabs::TabInterface::DetachReason reason);

  TabData tab_data_;

  raw_ptr<tabs::TabInterface> tab_interface_;
  base::CallbackListSubscription tab_ui_change_subscription_;
  base::CallbackListSubscription alert_change_subscription_;
  base::CallbackListSubscription pinned_state_change_subscription_;
  base::CallbackListSubscription blocked_state_change_subscription_;
  base::CallbackListSubscription tab_detached_subscription_;
  base::RepeatingCallbackList<void(TabChangeType, const TabData&)>
      tab_data_changed_callback_list_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_TAB_DATA_H_
