// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_DEVICE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_DEVICE_BUTTON_H_

#include <string>

#include "base/bind.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "components/sync/protocol/sync.pb.h"

namespace send_tab_to_self {

struct TargetDeviceInfo;

// A button representing a device in share bubble. It is highlighted when
// hovered.
class SendTabToSelfBubbleDeviceButton : public HoverButton {
 public:
  SendTabToSelfBubbleDeviceButton(views::ButtonListener* button_listener,
                                  const TargetDeviceInfo& device_info,
                                  int button_tag);
  ~SendTabToSelfBubbleDeviceButton() override;

  const std::string& device_name() const { return device_name_; }
  const std::string& device_guid() const { return device_guid_; }
  sync_pb::SyncEnums::DeviceType device_type() const { return device_type_; }

 private:
  std::string device_name_;
  std::string device_guid_;
  sync_pb::SyncEnums::DeviceType device_type_;

  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfBubbleDeviceButton);
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_DEVICE_BUTTON_H_
