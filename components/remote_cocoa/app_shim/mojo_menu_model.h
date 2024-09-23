// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_MOJO_MENU_MODEL_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_MOJO_MENU_MODEL_H_

#include "base/memory/weak_ptr.h"
#include "components/remote_cocoa/common/menu.mojom.h"
#include "ui/base/models/menu_model.h"

namespace remote_cocoa {

// MenuModel implementation that wraps a list of mojom::MenuItems. Supports only
// the functionality that is supported by `MenuControllerCocoa`.
// Additionally this menu model will always return false for IsItemDynamicAt.
// Instances of this model are never re-used for multiple showings of the same
// menu, so dynamic behavior would be limited to submenus anyway. And currently
// nowhere in Chrome are dynamic items used in submenus.
class MojoMenuModel final : public ui::MenuModel {
 public:
  MojoMenuModel(std::vector<mojom::MenuItemPtr> menu_items,
                mojom::MenuHost* menu_host);
  ~MojoMenuModel() override;

  // ui::MenuModel:
  base::WeakPtr<ui::MenuModel> AsWeakPtr() override;
  size_t GetItemCount() const override;
  ItemType GetTypeAt(size_t index) const override;
  ui::MenuSeparatorType GetSeparatorTypeAt(size_t index) const override;
  int GetCommandIdAt(size_t index) const override;
  std::u16string GetLabelAt(size_t index) const override;
  bool IsItemDynamicAt(size_t index) const override;
  bool MayHaveMnemonicsAt(size_t index) const override;
  bool GetAcceleratorAt(size_t index,
                        ui::Accelerator* accelerator) const override;
  bool IsItemCheckedAt(size_t index) const override;
  int GetGroupIdAt(size_t index) const override;
  ui::ImageModel GetIconAt(size_t index) const override;
  ui::ButtonMenuItemModel* GetButtonMenuItemAt(size_t index) const override;
  bool IsEnabledAt(size_t index) const override;
  bool IsVisibleAt(size_t index) const override;
  bool IsAlertedAt(size_t index) const override;
  bool IsNewFeatureAt(size_t index) const override;
  MojoMenuModel* GetSubmenuModelAt(size_t index) const override;
  void ActivatedAt(size_t index) override;
  void ActivatedAt(size_t index, int event_flags) override;

 private:
  mojom::MenuItemCommonFields* GetCommonFieldsAt(size_t index) const;

  std::vector<mojom::MenuItemPtr> menu_items_;
  raw_ptr<mojom::MenuHost> menu_host_;

  // MenuModel instances for sub-menus. Created on demand when GetSubmenuModelAt
  // is called.
  mutable std::vector<std::unique_ptr<MojoMenuModel>> submenus_;

  base::WeakPtrFactory<MojoMenuModel> weak_ptr_factory_{this};
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_MOJO_MENU_MODEL_H_
