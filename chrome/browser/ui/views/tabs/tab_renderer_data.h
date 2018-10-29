// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_RENDERER_DATA_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_RENDERER_DATA_H_

#include "base/strings/string16.h"
#include "chrome/browser/ui/tabs/tab_network_state.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/views/chrome_views_export.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

// Wraps the state needed by the renderers.
struct CHROME_VIEWS_EXPORT TabRendererData {
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

  gfx::ImageSkia favicon;
  TabNetworkState network_state = TabNetworkState::kNone;
  base::string16 title;
  GURL url;
  base::TerminationStatus crashed_status =
      base::TERMINATION_STATUS_STILL_RUNNING;
  bool incognito = false;
  bool show_icon = true;
  bool pinned = false;
  bool blocked = false;
  TabAlertState alert_state = TabAlertState::NONE;
  bool should_hide_throbber = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_RENDERER_DATA_H_
