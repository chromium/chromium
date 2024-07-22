// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ENTERPRISE_MANAGED_MENU_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_ENTERPRISE_MANAGED_MENU_VIEW_H_

#include <memory>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class Button;
}

class Browser;

// This bubble view is displayed when the user clicks on the management button
// displays the management menu.
class ManagedMenuView : public views::BubbleDialogDelegateView {
  METADATA_HEADER(ManagedMenuView, views::BubbleDialogDelegateView)

 public:
  ManagedMenuView(views::Button* anchor_button, Browser* browser);
  ManagedMenuView(const ManagedMenuView&) = delete;
  ManagedMenuView& operator=(const ManagedMenuView&) = delete;

  ~ManagedMenuView() override;

  void BuildView();
  void BuildInfoContainerBackground(const ui::ColorProvider* color_provider);

  // views::BubbleDialogDelegateView:
  void Init() final;
  void OnThemeChanged() override;

 private:
  int GetMaxHeight() const;
  // views::BubbleDialogDelegateView:
  std::u16string GetAccessibleWindowTitle() const override;

  base::RepeatingCallback<void(const ui::ColorProvider*)>
      info_container_background_callback_ = base::DoNothing();

  raw_ptr<views::View> info_container_ = nullptr;
  const raw_ptr<Browser> browser_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_ENTERPRISE_MANAGED_MENU_VIEW_H_
