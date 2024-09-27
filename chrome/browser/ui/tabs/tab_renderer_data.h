// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_RENDERER_DATA_H_
#define CHROME_BROWSER_UI_TABS_TAB_RENDERER_DATA_H_

#include <string>

#include "base/process/kill.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_network_state.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

class TabStripModel;
class TabResourceUsage;
class ThumbnailImage;

// Wraps the state needed by the renderers.
struct TabRendererData {
  static TabRendererData FromTabInModel(const TabStripModel* model, int index);

  TabRendererData();
  TabRendererData(const TabRendererData& other);
  TabRendererData(TabRendererData&& other);
  ~TabRendererData();

  TabRendererData& operator=(const TabRendererData& other);
  TabRendererData& operator=(TabRendererData&& other);

  bool operator==(const TabRendererData& other) const;

  // This interprets the crashed status to decide whether or not this
  // render data represents a tab that is "crashed" (i.e. the render
  // process died unexpectedly).
  bool IsCrashed() const;

  ui::ImageModel favicon;
  scoped_refptr<ThumbnailImage> thumbnail;
  TabNetworkState network_state = TabNetworkState::kNone;
  std::u16string title;
  // This corresponds to WebContents::GetVisibleUrl().
  GURL visible_url;
  // This corresponds to WebContents::GetLastCommittedUrl().
  GURL last_committed_url;
  // False if the omnibox doesn't display the URL (i.e. when a lookalike URL
  // interstitial is being displayed).
  bool should_display_url = true;
  base::TerminationStatus crashed_status =
      base::TERMINATION_STATUS_STILL_RUNNING;
  bool incognito = false;
  bool show_icon = true;
  bool pinned = false;
  bool blocked = false;
  std::vector<TabAlertState> alert_state;
  bool should_hide_throbber = false;
  bool should_render_empty_title = false;
  bool should_themify_favicon = false;
  bool is_tab_discarded = false;
  bool should_show_discard_status = false;
  // Amount of memory saved through discarding the tab
  int64_t discarded_memory_savings_in_bytes = 0;
  // Contains information about how much resource a tab is using
  scoped_refptr<const TabResourceUsage> tab_resource_usage;
  bool is_monochrome_favicon = false;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_RENDERER_DATA_H_
