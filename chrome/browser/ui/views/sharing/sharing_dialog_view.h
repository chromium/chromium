// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHARING_SHARING_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SHARING_SHARING_DIALOG_VIEW_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "components/sharing_message/sharing_dialog.h"
#include "components/sharing_message/sharing_dialog_data.h"

namespace views {
class StyledLabel;
class View;
}  // namespace views

enum class SharingDialogType;

class SharingDialogView : public SharingDialog,
                          public LocationBarBubbleDelegateView {
 public:
  // Bubble will be anchored to |anchor_view|.
  SharingDialogView(views::View* anchor_view,
                    content::WebContents* web_contents,
                    SharingDialogData data);

  SharingDialogView(const SharingDialogView&) = delete;
  SharingDialogView& operator=(const SharingDialogView&) = delete;

  ~SharingDialogView() override;

  // SharingDialog:
  void Hide() override;

  // LocationBarBubbleDelegateView:
  bool ShouldShowCloseButton() const override;
  std::u16string GetWindowTitle() const override;
  void WindowClosing() override;
  void WebContentsDestroyed() override;
  void AddedToWidget() override;

  static views::BubbleDialogDelegateView* GetAsBubble(SharingDialog* dialog);
  static views::BubbleDialogDelegateView* GetAsBubbleForClickToCall(
      SharingDialog* dialog);

  SharingDialogType GetDialogType() const;

  const View* button_list_for_testing() const { return button_list_; }

 private:
  friend class SharingDialogViewTest;

  // LocationBarBubbleDelegateView:
  void Init() override;

  // Populates the dialog view containing valid devices and apps.
  void InitListView();
  // Populates the dialog view containing error help text.
  void InitErrorView();

  std::unique_ptr<views::StyledLabel> CreateHelpText();

  void DeviceButtonPressed(size_t index);
  void AppButtonPressed(size_t index);

  SharingDialogData data_;

  // References to device and app buttons views.
  raw_ptr<View> button_list_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SHARING_SHARING_DIALOG_VIEW_H_
