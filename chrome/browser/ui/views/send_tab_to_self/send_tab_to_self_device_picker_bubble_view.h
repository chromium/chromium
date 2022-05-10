// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_DEVICE_PICKER_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_DEVICE_PICKER_BUBBLE_VIEW_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/media_router/cast_dialog_controller.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"

namespace content {
class WebContents;
}  // namespace content

namespace ui {
class Event;
}  // namespace ui

namespace send_tab_to_self {

class SendTabToSelfBubbleController;
class SendTabToSelfBubbleDeviceButton;

// View component of the send tab to self bubble that allows users to choose
// target device to send tab to.
class SendTabToSelfDevicePickerBubbleView
    : public SendTabToSelfBubbleView,
      public LocationBarBubbleDelegateView {
 public:
  // Bubble will be anchored to |anchor_view|.
  SendTabToSelfDevicePickerBubbleView(views::View* anchor_view,
                                      content::WebContents* web_contents);

  SendTabToSelfDevicePickerBubbleView(
      const SendTabToSelfDevicePickerBubbleView&) = delete;
  SendTabToSelfDevicePickerBubbleView& operator=(
      const SendTabToSelfDevicePickerBubbleView&) = delete;

  ~SendTabToSelfDevicePickerBubbleView() override;

  // SendTabToSelfBubbleView:
  void Hide() override;

  // views::WidgetDelegateView:
  bool ShouldShowCloseButton() const override;
  std::u16string GetWindowTitle() const override;
  void WindowClosing() override;

  void BackButtonPressed();

  void DeviceButtonPressed(SendTabToSelfBubbleDeviceButton* device_button);

  void OnManageDevicesClicked(const ui::Event& event);

  const views::View* GetButtonContainerForTesting() const;

 private:
  // views::BubbleDialogDelegateView:
  void Init() override;
  void AddedToWidget() override;

  // Creates the subtitle / hint text used in V2.
  void CreateHintTextLabel();

  // Creates the scroll view containing target devices.
  void CreateDevicesScrollView();

  // Creates the link leading to a page where the user can manage their known
  // target devices.
  void CreateManageDevicesLink();

  base::WeakPtr<SendTabToSelfBubbleController> controller_;

  // ScrollView containing the list of device buttons.
  // Only kept for GetButtonContainerForTesting().
  raw_ptr<views::ScrollView> scroll_view_ = nullptr;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_DEVICE_PICKER_BUBBLE_VIEW_H_
