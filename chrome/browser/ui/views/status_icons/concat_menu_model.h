// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_CONCAT_MENU_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_CONCAT_MENU_MODEL_H_

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"

// Combines two menu models (without using submenus).
class ConcatMenuModel final : public ui::MenuModel {
 public:
  ConcatMenuModel(ui::MenuModel* m1, ui::MenuModel* m2);

  ConcatMenuModel(const ConcatMenuModel&) = delete;
  ConcatMenuModel& operator=(const ConcatMenuModel&) = delete;

  ~ConcatMenuModel() override;

  // ui::MenuModel:
  base::WeakPtr<ui::MenuModel> AsWeakPtr() override;
  size_t GetItemCount() const override;
  ItemType GetTypeAt(size_t index) const override;
  ui::MenuSeparatorType GetSeparatorTypeAt(size_t index) const override;
  int GetCommandIdAt(size_t index) const override;
  std::u16string GetLabelAt(size_t index) const override;
  std::u16string GetMinorTextAt(size_t index) const override;
  ui::ImageModel GetMinorIconAt(size_t index) const override;
  bool IsItemDynamicAt(size_t index) const override;
  bool GetAcceleratorAt(size_t index,
                        ui::Accelerator* accelerator) const override;
  bool IsItemCheckedAt(size_t index) const override;
  int GetGroupIdAt(size_t index) const override;
  ui::ImageModel GetIconAt(size_t index) const override;
  ui::ButtonMenuItemModel* GetButtonMenuItemAt(size_t index) const override;
  bool IsEnabledAt(size_t index) const override;
  bool IsVisibleAt(size_t index) const override;
  void ActivatedAt(size_t index) override;
  void ActivatedAt(size_t index, int event_flags) override;
  MenuModel* GetSubmenuModelAt(size_t index) const override;
  void MenuWillShow() override;
  void MenuWillClose() override;

 private:
  template <typename F, typename... Ts>
  auto GetterImpl(F&& f, size_t index, Ts&&... args) const {
    return (GetMenuAndIndex(&index)->*f)(index, args...);
  }

  // Returns either |m1_| or |m2_| for the input |index|.  |index| will be
  // adjusted for the returned menu.
  ui::MenuModel* GetMenuAndIndex(size_t* index) const;

  const raw_ptr<ui::MenuModel, DanglingUntriaged> m1_;
  const raw_ptr<ui::MenuModel, DanglingUntriaged> m2_;

  base::WeakPtrFactory<ConcatMenuModel> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_CONCAT_MENU_MODEL_H_
