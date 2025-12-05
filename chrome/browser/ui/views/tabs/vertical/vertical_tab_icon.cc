// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_icon.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/views/view_class_properties.h"

VerticalTabIcon::VerticalTabIcon(const tabs::TabInterface* tab) {
  SetData(tab);
}

VerticalTabIcon::~VerticalTabIcon() = default;

void VerticalTabIcon::SetData(const tabs::TabInterface* tab) {
  const bool was_showing_load = GetShowingLoadingAnimation();

  int index =
      tab->GetBrowserWindowInterface()->GetTabStripModel()->GetIndexOfTab(tab);
  TabRendererData tab_renderer_data = TabRendererData::FromTabInModel(
      tab->GetBrowserWindowInterface()->GetTabStripModel(), index);

  themed_favicon_ = tab_renderer_data.favicon.Rasterize(GetColorProvider());
  SetNetworkState(tab_renderer_data.network_state);
  has_tab_renderer_data_ = true;

  const bool showing_load = GetShowingLoadingAnimation();

  RefreshLayer();

  if (was_showing_load && !showing_load) {
    // Loading animation transitioning from on to off.
    loading_animation_start_time_ = base::TimeTicks();
    SchedulePaint();
  } else if (!was_showing_load && showing_load) {
    // Loading animation transitioning from off to on. The animation painting
    // function will lazily initialize the data.
    SchedulePaint();
  }
}

BEGIN_METADATA(VerticalTabIcon)
END_METADATA
