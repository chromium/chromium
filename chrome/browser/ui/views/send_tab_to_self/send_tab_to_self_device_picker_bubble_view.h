// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_DEVICE_PICKER_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_DEVICE_PICKER_BUBBLE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace content {
class WebContents;
}  // namespace content

namespace send_tab_to_self {

class SendTabToSelfBubbleDeviceButton;

// A bubble dialog that allows users to select a target device to send a tab to.
class SendTabToSelfDevicePickerBubbleView : public SendTabToSelfBubbleView {
  METADATA_HEADER(SendTabToSelfDevicePickerBubbleView, SendTabToSelfBubbleView)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSendTabToSelfDevicePickerBubbleId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kManageDevicesLinkElementId);

  SendTabToSelfDevicePickerBubbleView(views::BubbleAnchor anchor,
                                      content::WebContents* web_contents);

  SendTabToSelfDevicePickerBubbleView(
      const SendTabToSelfDevicePickerBubbleView&) = delete;
  SendTabToSelfDevicePickerBubbleView& operator=(
      const SendTabToSelfDevicePickerBubbleView&) = delete;

  void SelectTargetDevice(SendTabToSelfBubbleDeviceButton* device_button);

  const views::View* GetButtonContainerForTesting() const;

 private:
  // views::BubbleDialogDelegateView:
  void Init() override;

  // Handles the "Send" button click.
  void HandleSendClicked();

  // Creates the scroll view containing target devices.
  void CreateDevicesScrollView();

  // ScrollView containing the list of device buttons.
  // Only kept for GetButtonContainerForTesting().
  raw_ptr<views::ScrollView> scroll_view_ = nullptr;

  // Currently selected device button.
  raw_ptr<SendTabToSelfBubbleDeviceButton> selected_button_ = nullptr;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_DEVICE_PICKER_BUBBLE_VIEW_H_
