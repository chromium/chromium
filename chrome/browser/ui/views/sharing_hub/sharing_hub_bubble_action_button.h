// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHARING_HUB_SHARING_HUB_BUBBLE_ACTION_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_SHARING_HUB_SHARING_HUB_BUBBLE_ACTION_BUTTON_H_

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/sharing_hub/sharing_hub_model.h"
#include "ui/views/controls/button/button.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace sharing_hub {

class SharingHubBubbleViewImpl;
struct SharingHubAction;

// A button representing an action in the Sharing Hub bubble.
class SharingHubBubbleActionButton : public views::Button {
  METADATA_HEADER(SharingHubBubbleActionButton, views::Button)

 public:
  SharingHubBubbleActionButton(SharingHubBubbleViewImpl* bubble,
                               const SharingHubAction& action_info);
  SharingHubBubbleActionButton(const SharingHubBubbleActionButton&) = delete;
  SharingHubBubbleActionButton& operator=(const SharingHubBubbleActionButton&) =
      delete;
  ~SharingHubBubbleActionButton() override;

  const SharingHubAction& action_info() const { return action_info_; }

  // views::Button:
  // Listeners for various events, which this class uses to keep its visuals
  // consistent.
  void OnThemeChanged() override;
  void StateChanged(views::Button::ButtonState old_state) override;
  void OnFocus() override;
  void OnBlur() override;

 private:
  const SharingHubAction action_info_;

  raw_ptr<views::Label> title_;
  raw_ptr<views::ImageView> image_;

  void UpdateColors();
};

}  // namespace sharing_hub

#endif  // CHROME_BROWSER_UI_VIEWS_SHARING_HUB_SHARING_HUB_BUBBLE_ACTION_BUTTON_H_
