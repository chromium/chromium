// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_SHELF_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_ASH_SHELF_SHELF_CONTEXT_MENU_H_

#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/shelf_item.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/vector_icon_types.h"

class ChromeShelfController;

// ElementIdentifier for the shelf context menu "Close" menu item.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kShelfCloseMenuItem);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kShelfContextMenuNewWindowMenuItem);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kShelfContextMenuIncognitoWindowMenuItem);

// A base class for browser, extension, and ARC shelf item context menus.
class ShelfContextMenu : public ui::SimpleMenuModel::Delegate {
 public:
  ShelfContextMenu(const ShelfContextMenu&) = delete;
  ShelfContextMenu& operator=(const ShelfContextMenu&) = delete;

  ~ShelfContextMenu() override;

  // Static function to create a context menu instance.
  static std::unique_ptr<ShelfContextMenu> Create(
      ChromeShelfController* controller,
      const ash::ShelfItem* item,
      int64_t display_id);

  std::unique_ptr<ui::SimpleMenuModel> GetBaseMenuModel();

  using GetMenuModelCallback =
      base::OnceCallback<void(std::unique_ptr<ui::SimpleMenuModel>)>;
  virtual void GetMenuModel(GetMenuModelCallback callback) = 0;

  // ui::SimpleMenuModel::Delegate overrides:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  // Helper method to get the gfx::VectorIcon for a |type|. Returns an empty
  // gfx::VectorIcon if there is no icon for this |type|.
  const gfx::VectorIcon& GetCommandIdVectorIcon(int type, int string_id) const;

 protected:
  ShelfContextMenu(ChromeShelfController* controller,
                   const ash::ShelfItem* item,
                   int64_t display_id);

  ChromeShelfController* controller() const { return controller_; }
  const ash::ShelfItem& item() const { return item_; }

  // Add menu item for pin/unpin.
  void AddPinMenu(ui::SimpleMenuModel* menu_model);

  // Helper method to execute common commands. Returns true if handled.
  bool ExecuteCommonCommand(int command_id, int event_flags);

  // Helper method to add touchable or normal context menu options.
  void AddContextMenuOption(ui::SimpleMenuModel* menu_model,
                            ash::CommandId type,
                            int string_id);

  // Helper method to add element identifiers to some menu items.
  void MaybeSetElementIdentifier(ui::SimpleMenuModel* menu_model,
                                 ash::CommandId type);

  int64_t display_id() const { return display_id_; }

 private:
  raw_ptr<ChromeShelfController> controller_;

  const ash::ShelfItem item_;

  const int64_t display_id_;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_SHELF_CONTEXT_MENU_H_
