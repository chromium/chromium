// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHARING_SHARING_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SHARING_SHARING_DIALOG_VIEW_H_

#include <memory>
#include <vector>

#include "chrome/browser/sharing/sharing_dialog.h"
#include "chrome/browser/sharing/sharing_dialog_data.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/views/controls/button/button.h"

namespace views {
class StyledLabel;
class View;
}  // namespace views

class HoverButton;
enum class SharingDialogType;

class SharingDialogView : public SharingDialog,
                          public views::ButtonListener,
                          public LocationBarBubbleDelegateView {
 public:
  // Bubble will be anchored to |anchor_view|.
  SharingDialogView(views::View* anchor_view,
                    content::WebContents* web_contents,
                    SharingDialogData data);

  ~SharingDialogView() override;

  // SharingDialog:
  void Hide() override;

  // LocationBarBubbleDelegateView:
  bool ShouldShowCloseButton() const override;
  base::string16 GetWindowTitle() const override;
  void WindowClosing() override;
  void WebContentsDestroyed() override;
  gfx::Size CalculatePreferredSize() const override;
  void AddedToWidget() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  static views::BubbleDialogDelegateView* GetAsBubble(SharingDialog* dialog);

  static views::BubbleDialogDelegateView* GetAsBubbleForClickToCall(
      SharingDialog* dialog);

 private:
  friend class SharingDialogViewTest;
  FRIEND_TEST_ALL_PREFIXES(SharingDialogViewTest, PopulateDialogView);
  FRIEND_TEST_ALL_PREFIXES(SharingDialogViewTest, DevicePressed);
  FRIEND_TEST_ALL_PREFIXES(SharingDialogViewTest, AppPressed);
  FRIEND_TEST_ALL_PREFIXES(SharingDialogViewTest, HelpTextClickedEmpty);
  FRIEND_TEST_ALL_PREFIXES(SharingDialogViewTest, HelpTextClickedOnlyApps);
  FRIEND_TEST_ALL_PREFIXES(SharingDialogViewTest, ThemeChangedEmptyList);

  FRIEND_TEST_ALL_PREFIXES(ClickToCallBrowserTest, LeftClick_ChooseDevice);

  SharingDialogType GetDialogType() const;

  // LocationBarBubbleDelegateView:
  void Init() override;

  // Populates the dialog view containing valid devices and apps.
  void InitListView();
  // Populates the dialog view containing error help text.
  void InitErrorView();

  std::unique_ptr<views::StyledLabel> CreateHelpText();

  SharingDialogData data_;

  // References to device and app buttons views.
  std::vector<HoverButton*> dialog_buttons_;

  DISALLOW_COPY_AND_ASSIGN(SharingDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SHARING_SHARING_DIALOG_VIEW_H_
