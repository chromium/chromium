// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_H_
#define COMPONENTS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_H_

#include "ui/base/models/menu_model_delegate.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/menu/menu_model_adapter.h"

namespace views {
class Widget;
class MenuItemView;
}  // namespace views

namespace arc {

class ArcResizeLockPrefDelegate;

class ResizeToggleMenu : public ui::SimpleMenuModel::Delegate {
 public:
  enum CommandId {
    kResizePhone = 1,  // Starting from 1 to avoid the conflict with "separator
                       // item" because its command id is 0.
    kResizeTablet = 2,
    kResizeDesktop = 3,
    kOpenSettings = 4,
    kMaxValue = kOpenSettings,
  };

  ResizeToggleMenu(views::Widget* widget,
                   ArcResizeLockPrefDelegate* pref_delegate);
  ResizeToggleMenu(const ResizeToggleMenu&) = delete;
  ResizeToggleMenu& operator=(const ResizeToggleMenu&) = delete;
  ~ResizeToggleMenu() override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  friend class ResizeToggleMenuTest;

  std::unique_ptr<ui::SimpleMenuModel> MakeMenuModel();

  views::Widget* widget_;

  ArcResizeLockPrefDelegate* pref_delegate_;

  // Owned by |menu_runner_|. Store this here only for testing.
  views::MenuItemView* root_view_ = nullptr;

  std::unique_ptr<ui::SimpleMenuModel> model_;
  std::unique_ptr<views::MenuModelAdapter> adapter_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_H_
