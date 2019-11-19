// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_CONCAT_MENU_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_CONCAT_MENU_MODEL_H_

#include <utility>

#include "base/macros.h"
#include "ui/base/models/menu_model.h"

// Combines two menu models (without using submenus).
class ConcatMenuModel : public ui::MenuModel {
 public:
  ConcatMenuModel(ui::MenuModel* m1, ui::MenuModel* m2);
  ~ConcatMenuModel() override;

  // MenuModel:
  bool HasIcons() const override;
  int GetItemCount() const override;
  ItemType GetTypeAt(int index) const override;
  ui::MenuSeparatorType GetSeparatorTypeAt(int index) const override;
  int GetCommandIdAt(int index) const override;
  base::string16 GetLabelAt(int index) const override;
  base::string16 GetMinorTextAt(int index) const override;
  const gfx::VectorIcon* GetMinorIconAt(int index) const override;
  bool IsItemDynamicAt(int index) const override;
  bool GetAcceleratorAt(int index, ui::Accelerator* accelerator) const override;
  bool IsItemCheckedAt(int index) const override;
  int GetGroupIdAt(int index) const override;
  bool GetIconAt(int index, gfx::Image* icon) const override;
  ui::ButtonMenuItemModel* GetButtonMenuItemAt(int index) const override;
  bool IsEnabledAt(int index) const override;
  bool IsVisibleAt(int index) const override;
  void ActivatedAt(int index) override;
  void ActivatedAt(int index, int event_flags) override;
  MenuModel* GetSubmenuModelAt(int index) const override;
  void MenuWillShow() override;
  void MenuWillClose() override;

 private:
  template <typename F, typename... Ts>
  auto GetterImpl(F&& f, int index, Ts&&... args) const {
    return (GetMenuAndIndex(&index)->*f)(index, args...);
  }

  // Returns either |m1_| or |m2_| for the input |index|.  |index| will be
  // adjusted for the returned menu.
  ui::MenuModel* GetMenuAndIndex(int* index) const;

  ui::MenuModel* const m1_;
  ui::MenuModel* const m2_;

  DISALLOW_COPY_AND_ASSIGN(ConcatMenuModel);
};

#endif  // CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_CONCAT_MENU_MODEL_H_
