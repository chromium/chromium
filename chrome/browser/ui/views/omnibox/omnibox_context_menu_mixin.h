// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_CONTEXT_MENU_MIXIN_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_CONTEXT_MENU_MIXIN_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

class LocationBar;
class OmniboxController;

namespace ai_mode_button_config {
struct AiModeButtonConfig;
}  // namespace ai_mode_button_config

namespace gfx {
class FontList;
}  // namespace gfx

namespace send_tab_to_self {
class SendTabToSelfContextMenuDelegate;
}  // namespace send_tab_to_self

namespace ui {
class SimpleMenuModel;
}  // namespace ui

class OmniboxContextMenuMixinBase {
 public:
  void SetClipboardTextForTesting(const std::u16string& text) {
    clipboard_text_for_menu_ = text;
  }

 protected:
  // `location_bar` may be null in tests.
  OmniboxContextMenuMixinBase(LocationBar* location_bar,
                              OmniboxController* controller);
  ~OmniboxContextMenuMixinBase();

  // Adds Omnibox specific items to `menu_contents`, assuming the basic
  // editing commands (or at least ui::TouchEditable::MenuCommands::kPaste)
  // have already been added.
  void AddOmniboxSpecificItems(ui::SimpleMenuModel* menu_contents);

  bool HandleIsCommandIdChecked(int id) const;
  bool HandleIsItemForCommandIdDynamic(int command_id) const;
  std::u16string HandleGetLabelForCommandId(int command_id) const;
  bool HandleIsCommandIdEnabled(int command_id) const;
  bool HandleExecuteCommand(int command_id, int event_flags);

  // Asynchronously calls `closure` once preparations to show the context
  // menu (examining the clipboard) have been done.
  void PrepareToShowContextMenu(base::OnceClosure closure);

  // This should be overridden, and return true if the omnibox isn't editable
  // (e.g. for window.open created popups).
  virtual bool IsContextMenuForReadOnlyOmnibox() const = 0;

  // Returns fonts to use for context menu. Used to elide long entries.
  virtual const gfx::FontList& FontListForContextMenu() const = 0;

  // Should return true if basic editing command (e.g. copy/paste) should be
  // enabled in context menu.
  virtual bool IsContextMenuTextEditingCommandEnabled(int command_id) const = 0;

 private:
  void OnGotClipboardText(base::OnceClosure closure, std::u16string text);

  // Helper that adds a menu entry to send current tab to other devices if
  // appropriate.
  void MaybeAddSendTabToSelfItem(ui::SimpleMenuModel* menu_contents);
  void BuildSendTabToSelfSubmenu(ui::SimpleMenuModel* menu_contents,
                                 size_t index);
  void BuildSendTabToSelfSimpleItem(ui::SimpleMenuModel* menu_contents,
                                    size_t index);

  const ai_mode_button_config::AiModeButtonConfig* GetAiModeConfig() const;

  raw_ptr<LocationBar> location_bar_;
  raw_ptr<OmniboxController> controller_;

  std::unique_ptr<send_tab_to_self::SendTabToSelfContextMenuDelegate>
      send_tab_to_self_submenu_delegate_;
  std::unique_ptr<ui::SimpleMenuModel> send_tab_to_self_submenu_;

  // Cached clipboard text for menu paste state.
  // Updated by PrepareToShowContextMenu()
  std::u16string clipboard_text_for_menu_;

  base::WeakPtrFactory<OmniboxContextMenuMixinBase> weak_ptr_factory_{this};
};

// Base should be ui::SimpleMenuModel::Delegate or a subclass.
template <typename Base>
class OmniboxContextMenuMixin : public Base,
                                public OmniboxContextMenuMixinBase {
 public:
  OmniboxContextMenuMixin(LocationBar* location_bar,
                          OmniboxController* controller)
      : OmniboxContextMenuMixinBase(location_bar, controller) {}

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int id) const override {
    return HandleIsCommandIdChecked(id);
  }

  bool IsItemForCommandIdDynamic(int command_id) const override {
    return HandleIsItemForCommandIdDynamic(command_id);
  }

  std::u16string GetLabelForCommandId(int command_id) const override {
    return HandleGetLabelForCommandId(command_id);
  }

  bool IsCommandIdEnabled(int command_id) const override {
    return HandleIsCommandIdEnabled(command_id);
  }

  void ExecuteCommand(int command_id, int event_flags) override {
    HandleExecuteCommand(command_id, event_flags);
  }
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_CONTEXT_MENU_MIXIN_H_
