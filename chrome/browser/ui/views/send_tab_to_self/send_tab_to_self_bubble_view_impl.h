// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_VIEW_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_VIEW_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/media_router/cast_dialog_controller.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/views/controls/button/button.h"

namespace gfx {
class Canvas;
}  // namespace gfx

namespace content {
class WebContents;
}  // namespace content

namespace send_tab_to_self {

class SendTabToSelfBubbleController;
class SendTabToSelfBubbleDeviceButton;
struct TargetDeviceInfo;

// View component of the send tab to self bubble that allows users to choose
// target device to send tab to.
class SendTabToSelfBubbleViewImpl : public SendTabToSelfBubbleView,
                                    public views::ButtonListener,
                                    public LocationBarBubbleDelegateView {
 public:
  // Bubble will be anchored to |anchor_view|.
  SendTabToSelfBubbleViewImpl(views::View* anchor_view,
                              content::WebContents* web_contents,
                              SendTabToSelfBubbleController* controller);

  ~SendTabToSelfBubbleViewImpl() override;

  // SendTabToSelfBubbleView:
  void Hide() override;

  // views::WidgetDelegateView:
  bool ShouldShowCloseButton() const override;
  base::string16 GetWindowTitle() const override;
  void WindowClosing() override;

  // views::DialogDelegate:
  bool Close() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;

  // Shows the bubble view.
  void Show(DisplayReason reason);

  // Called by tests.
  const std::vector<SendTabToSelfBubbleDeviceButton*>&
  GetDeviceButtonsForTest();

 private:
  friend class SendTabToSelfBubbleViewImplTest;
  FRIEND_TEST_ALL_PREFIXES(SendTabToSelfBubbleViewImplTest, PopulateScrollView);
  FRIEND_TEST_ALL_PREFIXES(SendTabToSelfBubbleViewImplTest, DevicePressed);

  // views::BubbleDialogDelegateView:
  void Init() override;

  // Creates the scroll view.
  void CreateScrollView();

  // Populates the scroll view containing valid devices.
  void PopulateScrollView(const std::vector<TargetDeviceInfo>& devices);

  // Handles the action when a target device has been pressed.
  void DevicePressed(size_t index);

  // Resizes and potentially moves the bubble to fit the content's preferred
  // size.
  void MaybeSizeToContents();

  content::WebContents* web_contents_;         // Weak reference.
  SendTabToSelfBubbleController* controller_;  // Weak reference.

  // Title shown at the top of the bubble.
  base::string16 bubble_title_;

  // Contains references to device buttons in the order they appear.
  std::vector<SendTabToSelfBubbleDeviceButton*> device_buttons_;

  // ScrollView containing the list of device buttons.
  views::ScrollView* scroll_view_ = nullptr;

  // The device that the user has selected to share tab to.
  base::Optional<size_t> selected_device_index_;

  base::WeakPtrFactory<SendTabToSelfBubbleViewImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfBubbleViewImpl);
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_VIEW_IMPL_H_
