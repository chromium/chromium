// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_DEVICE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_DEVICE_BUTTON_H_

#include <string>

#include "base/functional/bind.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "components/sync_device_info/device_info.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace send_tab_to_self {

class SendTabToSelfDevicePickerBubbleView;
struct TargetDeviceInfo;

// A button representing a device in share bubble. It is highlighted when
// hovered.
class SendTabToSelfBubbleDeviceButton : public HoverButton {
  METADATA_HEADER(SendTabToSelfBubbleDeviceButton, HoverButton)

 public:
  SendTabToSelfBubbleDeviceButton(SendTabToSelfDevicePickerBubbleView* bubble,
                                  const TargetDeviceInfo& device_info);
  SendTabToSelfBubbleDeviceButton(const SendTabToSelfBubbleDeviceButton&) =
      delete;
  SendTabToSelfBubbleDeviceButton& operator=(
      const SendTabToSelfBubbleDeviceButton&) = delete;
  ~SendTabToSelfBubbleDeviceButton() override;

  const std::string& device_name() const { return device_name_; }
  const std::string& device_guid() const { return device_guid_; }

 private:
  std::string device_name_;
  std::string device_guid_;
  syncer::DeviceInfo::FormFactor device_form_factor_;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_DEVICE_BUTTON_H_
