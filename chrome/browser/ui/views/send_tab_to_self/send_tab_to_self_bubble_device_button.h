// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_DEVICE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_DEVICE_BUTTON_H_

#include <string>

#include "base/bind.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "components/sync/protocol/sync.pb.h"
#include "ui/views/metadata/metadata_header_macros.h"

namespace send_tab_to_self {

class SendTabToSelfBubbleViewImpl;
struct TargetDeviceInfo;

// A button representing a device in share bubble. It is highlighted when
// hovered.
class SendTabToSelfBubbleDeviceButton : public HoverButton {
 public:
  METADATA_HEADER(SendTabToSelfBubbleDeviceButton);
  SendTabToSelfBubbleDeviceButton(SendTabToSelfBubbleViewImpl* bubble,
                                  const TargetDeviceInfo& device_info);
  SendTabToSelfBubbleDeviceButton(const SendTabToSelfBubbleDeviceButton&) =
      delete;
  SendTabToSelfBubbleDeviceButton& operator=(
      const SendTabToSelfBubbleDeviceButton&) = delete;
  ~SendTabToSelfBubbleDeviceButton() override;

  const std::string& device_name() const { return device_name_; }
  const std::string& device_guid() const { return device_guid_; }
  sync_pb::SyncEnums::DeviceType device_type() const { return device_type_; }

 private:
  std::string device_name_;
  std::string device_guid_;
  sync_pb::SyncEnums::DeviceType device_type_;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_DEVICE_BUTTON_H_
