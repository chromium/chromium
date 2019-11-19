// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CLOUD_SERVICES_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CLOUD_SERVICES_DIALOG_VIEW_H_

#include "base/strings/string16.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/styled_label_listener.h"

class Browser;

namespace media_router {

// Dialog that asks the user whether they want to enable cloud services for the
// Cast feature.
class CloudServicesDialogView : public views::BubbleDialogDelegateView,
                                public views::StyledLabelListener {
 public:
  // Instantiates and shows the singleton dialog.
  static void ShowDialog(views::View* anchor_view, Browser* browser);

  // No-op if the dialog is currently not shown.
  static void HideDialog();

  static bool IsShowing();

  // Called by tests. Returns the singleton dialog instance.
  static CloudServicesDialogView* GetDialogForTest();

  // views::WidgetDelegateView:
  bool ShouldShowCloseButton() const override;

  // views::WidgetDelegate:
  base::string16 GetWindowTitle() const override;

  // views::DialogDelegate:
  bool Accept() override;
  bool Cancel() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

 private:
  CloudServicesDialogView(views::View* anchor_view, Browser* browser);
  ~CloudServicesDialogView() override;

  // views::BubbleDialogDelegateView:
  void Init() override;
  void WindowClosing() override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  // The singleton dialog instance. This is a nullptr when a dialog is not
  // shown.
  static CloudServicesDialogView* instance_;

  // Browser window that this dialog is attached to.
  Browser* const browser_;

  DISALLOW_COPY_AND_ASSIGN(CloudServicesDialogView);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CLOUD_SERVICES_DIALOG_VIEW_H_
