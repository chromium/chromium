// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_CONTEXT_MENU_MIXIN_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_CONTEXT_MENU_MIXIN_H_

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/ui_features.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/controls/textfield/textfield.h"

class LocationBar;
class OmniboxController;

struct AiModeButtonUiConfig;

namespace content {
struct ContextMenuParams;
class WebContents;
}  // namespace content

namespace gfx {
class FontList;
}  // namespace gfx

namespace send_tab_to_self {
class SendTabToSelfContextMenuDelegate;
}  // namespace send_tab_to_self

namespace ui {
class SimpleMenuModel;
}  // namespace ui

namespace views {
class Widget;
}  // namespace views

class OmniboxContextMenuMixinBase {
 public:
  // A few context menu items get IDs for interactive UI test use.
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCopyMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kShowFullUrlsMenuItem);

  void SetClipboardTextForTesting(const std::u16string& text) {
    clipboard_text_for_menu_ = text;
  }

  // Should return the widget the text services menu should use.
  // Need to be overridden if `AddTextfieldItems` is in use.
  virtual views::Widget* GetWidgetForTextServices();

 protected:
  // `location_bar` may be null in tests.
  OmniboxContextMenuMixinBase(LocationBar* location_bar,
                              OmniboxController* controller);
  ~OmniboxContextMenuMixinBase();

  // Adds standard views textfield menu items to `menu`, using a hidden
  // Views textfield to help manage that.
  void AddTextfieldItems(base::WeakPtr<content::WebContents> web_contents,
                         const content::ContextMenuParams& menu_params,
                         ui::SimpleMenuModel* menu_contents);

  // Adds Omnibox specific items to `menu_contents`, assuming the basic
  // editing commands (or at least ui::TouchEditable::MenuCommands::kPaste and
  // kUndo) have already been added.
  void AddOmniboxSpecificItems(ui::SimpleMenuModel* menu_contents);

  // These are automatically called by the subclass:
  bool HandleIsCommandIdChecked(int id) const;
  bool HandleIsItemForCommandIdDynamic(int command_id) const;
  std::u16string HandleGetLabelForCommandId(int command_id) const;
  bool HandleIsCommandIdEnabled(int command_id) const;

  // These need to be called when appropriate:
  // Handles omnibox (and text services) commands. Returns true if it
  // recognized `command_id` as such.
  bool HandleExecuteCommand(int command_id, int event_flags);

  // Handles basic text commands (undo/copy/paste/etc) by asking WebContents.
  void HandleExecuteTextEditingCommandOnWebContents(
      content::WebContents* web_contents,
      int command_id,
      int event_flags);

  bool HandleIsContextMenuTextEditingCommandEnabled(
      int command_id,
      const content::ContextMenuParams& menu_params) const;

  bool HandleGetAcceleratorForCommandId(int command_id,
                                        ui::Accelerator* accelerator) const;

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

  const AiModeButtonUiConfig* GetAiModeUiConfig() const;

  raw_ptr<LocationBar> location_bar_;
  raw_ptr<OmniboxController> controller_;

  std::unique_ptr<send_tab_to_self::SendTabToSelfContextMenuDelegate>
      send_tab_to_self_submenu_delegate_;
  std::unique_ptr<ui::SimpleMenuModel> send_tab_to_self_submenu_;

  // A helper `Textfield` instance used solely for generating and handling the
  // native context menu when the user right-clicks inside the full WebUI
  // popup's "input row" (i.e. this `Textfield` is NOT painted on-screen).
  //
  // Although the "input row" is rendered in WebUI, we intercept right-clicks to
  // show a native context menu that matches the `OmniboxViewViews` context
  // menu (`HandleContextMenu()`). Without this helper, populating standard text
  // editing items (Undo, Cut, Select All, Emoji, Look Up, etc.) across all
  // platforms would require duplicating substantial native context menu
  // construction logic.
  //
  // Thus, by maintaining this proxy `Textfield` instance, we can call
  // `UpdateContextMenuContents()` to generate the core native context menu
  // structure and delegate any auxiliary or platform-specific command
  // execution/enablement to `context_menu_textfield_helper_` when those
  // commands are not directly handled by the WebUI itself.
  std::unique_ptr<views::Textfield> context_menu_textfield_helper_;
  std::unique_ptr<views::ViewsTextServicesContextMenu>
      text_services_context_menu_;  // refs `context_menu_textfield_helper_`

  // Cached clipboard text for menu paste state.
  // Updated by PrepareToShowContextMenu()
  std::u16string clipboard_text_for_menu_;

  base::WeakPtrFactory<OmniboxContextMenuMixinBase> weak_ptr_factory_{this};
};

// Base should inherit off ui::SimpleMenuModel::Delegate.
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
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_CONTEXT_MENU_MIXIN_H_
