// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_ACTION_TOOLBAR_BUTTON_MENU_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_ACTION_TOOLBAR_BUTTON_MENU_MODEL_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/actions/actions.h"
#include "ui/base/models/menu_model.h"
#include "ui/views/view_class_properties.h"

class BrowserWindowInterface;

extern const ui::ClassProperty<actions::ActionId>* const kActionIdKey;

// PinnedActionToolbarButtonMenuModel is the interface for the showing of the
// context menu for the buttons in the PinnedToolbarActionsContainer, the
// context menu is created using the ActionItem's child actions as well as any
// default context menu items that should be visible for all
// PinnedActionToolbarButtons.
class PinnedActionToolbarButtonMenuModel final : public ui::MenuModel {
 public:
  PinnedActionToolbarButtonMenuModel(BrowserWindowInterface* browser_interface,
                                     actions::ActionId action_id);

  PinnedActionToolbarButtonMenuModel(
      const PinnedActionToolbarButtonMenuModel&) = delete;
  PinnedActionToolbarButtonMenuModel& operator=(
      const PinnedActionToolbarButtonMenuModel&) = delete;

  ~PinnedActionToolbarButtonMenuModel() override;

  // ui::MenuModel:
  base::WeakPtr<ui::MenuModel> AsWeakPtr() override;
  size_t GetItemCount() const override;
  ItemType GetTypeAt(size_t index) const override;
  ui::MenuSeparatorType GetSeparatorTypeAt(size_t index) const override;
  int GetCommandIdAt(size_t index) const override;
  std::u16string GetLabelAt(size_t index) const override;
  bool IsItemDynamicAt(size_t index) const override;
  bool GetAcceleratorAt(size_t index,
                        ui::Accelerator* accelerator) const override;
  bool IsItemCheckedAt(size_t index) const override;
  int GetGroupIdAt(size_t index) const override;
  ui::ImageModel GetIconAt(size_t index) const override;
  ui::ButtonMenuItemModel* GetButtonMenuItemAt(size_t index) const override;
  bool IsEnabledAt(size_t index) const override;
  bool IsVisibleAt(size_t index) const override;
  MenuModel* GetSubmenuModelAt(size_t index) const override;
  void ActivatedAt(size_t index) override;
  void ActivatedAt(size_t index, int event_flags) override;

  actions::ActionId GetActionIdAtForTesting(size_t index);

 private:
  struct Item {
    Item(Item&&);
    Item(actions::ActionId action_id, ItemType type);
    explicit Item(ItemType type);
    Item& operator=(Item&&);
    ~Item();

    actions::ActionId action_id = 0;
    ItemType type = TYPE_COMMAND;
  };

  actions::ActionItem* GetActionItemFor(actions::ActionId id) const;

  const raw_ptr<BrowserWindowInterface> browser_;
  actions::ActionId action_id_;
  std::vector<Item> items_;

  base::WeakPtrFactory<PinnedActionToolbarButtonMenuModel> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_ACTION_TOOLBAR_BUTTON_MENU_MODEL_H_
