// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_METRICS_UTIL_H_
#define COMPONENTS_SEND_TAB_TO_SELF_METRICS_UTIL_H_

namespace send_tab_to_self {

enum class ShareEntryPoint {
  kContentMenu,
  kLinkMenu,
  kOmniboxIcon,
  kOmniboxMenu,
  kShareMenu,
  kShareSheet,
  kTabMenu,
};

// Records whether the user clicked to send a tab to a device.
void RecordDeviceClicked(ShareEntryPoint entry_point);

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_METRICS_UTIL_H_
