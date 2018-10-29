// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_renderer_data.h"

#include "base/process/kill.h"
#include "build/build_config.h"

TabRendererData::TabRendererData() = default;
TabRendererData::TabRendererData(const TabRendererData& other) = default;
TabRendererData::TabRendererData(TabRendererData&& other) = default;

TabRendererData& TabRendererData::operator=(const TabRendererData& other) =
    default;
TabRendererData& TabRendererData::operator=(TabRendererData&& other) = default;

TabRendererData::~TabRendererData() = default;

bool TabRendererData::operator==(const TabRendererData& other) const {
  return favicon.BackedBySameObjectAs(other.favicon) &&
         network_state == other.network_state && title == other.title &&
         url == other.url && crashed_status == other.crashed_status &&
         incognito == other.incognito && show_icon == other.show_icon &&
         pinned == other.pinned && blocked == other.blocked &&
         alert_state == other.alert_state &&
         should_hide_throbber == other.should_hide_throbber;
}

bool TabRendererData::IsCrashed() const {
  return (crashed_status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED ||
#if defined(OS_CHROMEOS)
          crashed_status ==
              base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM ||
#endif
          crashed_status == base::TERMINATION_STATUS_PROCESS_CRASHED ||
          crashed_status == base::TERMINATION_STATUS_ABNORMAL_TERMINATION ||
          crashed_status == base::TERMINATION_STATUS_LAUNCH_FAILED);
}
