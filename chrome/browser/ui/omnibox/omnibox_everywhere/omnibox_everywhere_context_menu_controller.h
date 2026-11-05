// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_CONTEXT_MENU_CONTROLLER_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_CONTEXT_MENU_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/public/browser/context_menu_params.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/menu/menu_types.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
class MenuRunner;
class Widget;
}  // namespace views

namespace omnibox_everywhere {

// Builds, shows and dispatches commands for the native right-click context
// menu of the desktop Omnibox Everywhere widget. Owned by
// `OmniboxEverywhereUIManager`, which supplies the hosting widget,
// `WebContents` and profile through `Delegate`.
class OmniboxEverywhereContextMenuController
    : public ui::SimpleMenuModel::Delegate,
      public views::ContextMenuController {
 public:
  enum CommandId {
    kUndo = 1,
    kCut = 2,
    kCopy = 3,
    kPaste = 4,
    kPasteAndSearch = 5,
    kDelete = 6,
    kSelectAll = 7,
    kManageSearchEngines = 8,
    kAlwaysShowAiMode = 9,
    kShowShortcuts = 10,
    kCustomizeKeyboardShortcut = 11,
    kSettings = 12,
    kMinimize = 13,
    kClose = 14,
    kEmoji = 15,
#if BUILDFLAG(IS_MAC)
    kWritingDirectionMenu = 16,
    kWritingDirectionDefault = 17,
    kWritingDirectionLtr = 18,
    kWritingDirectionRtl = 19,
#endif  // BUILDFLAG(IS_MAC)
  };

  class Delegate {
   public:
    // The widget the menu is anchored to. May return null if the widget has
    // not been created yet or is being torn down.
    virtual views::Widget* GetContextMenuHostWidget() = 0;
    // The `WebContents` that editing commands are dispatched to.
    virtual content::WebContents* GetContextMenuWebContents() = 0;
    virtual Profile* GetContextMenuProfile() = 0;
    // Called after the menu has closed and its runner has been released.
    virtual void OnContextMenuClosed() = 0;
#if BUILDFLAG(IS_WIN)
    virtual void MinimizeFromContextMenu() = 0;
    virtual void CloseFromContextMenu() = 0;
#endif  // BUILDFLAG(IS_WIN)

   protected:
    virtual ~Delegate() = default;
  };

  explicit OmniboxEverywhereContextMenuController(Delegate& delegate);
  OmniboxEverywhereContextMenuController(
      const OmniboxEverywhereContextMenuController&) = delete;
  OmniboxEverywhereContextMenuController& operator=(
      const OmniboxEverywhereContextMenuController&) = delete;
  ~OmniboxEverywhereContextMenuController() override;

  // Shows a lightweight context menu for `params`. Reads the clipboard first
  // (asynchronously) when the target is editable so that the Paste and Search
  // / Paste and Go label can reflect the clipboard contents.
  void HandleContextMenu(const content::ContextMenuParams& params);

  // Shows the overflow (three-dot) menu with the same items as the background
  // right-click menu, anchored below `anchor_screen_rect` (screen DIPs).
  // `source_type` is how the menu was triggered; a keyboard-triggered menu
  // selects its first item.
  // `on_closed` runs once when the menu closes or is replaced, or immediately
  // if the menu cannot be shown.
  void ShowOverflowMenu(const gfx::Rect& anchor_screen_rect,
                        ui::mojom::MenuSourceType source_type,
                        base::OnceClosure on_closed);

  // Returns true while the menu is showing.
  bool IsOpen() const { return is_open_; }

  // Cancels the menu if it is showing. The close callback still runs.
  void Cancel();

  // Cancels the menu and releases the runner, models and last params. Used
  // when the hosting widget is destroyed.
  void Reset();

  // ui::SimpleMenuModel::Delegate:
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdChecked(int command_id) const override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override;

  // For testing:
  // Factory callback used in tests to mock `views::MenuRunner` creation.
  using MenuRunnerFactory = base::RepeatingCallback<std::unique_ptr<
      views::MenuRunner>(ui::MenuModel*, base::RepeatingClosure)>;
  void SetMenuRunnerFactoryForTesting(MenuRunnerFactory factory) {
    menu_runner_factory_ = std::move(factory);
  }
  const ui::SimpleMenuModel* model_for_testing() const {
    return context_menu_model_.get();
  }

 private:
  void ShowContextMenu(const content::ContextMenuParams& params,
                       std::u16string clipboard_text);
  // Creates the runner for `context_menu_model_` and runs it.
  void RunMenu(views::Widget* host_widget,
               const gfx::Rect& anchor_screen_rect,
               views::MenuAnchorPosition anchor_position,
               ui::mojom::MenuSourceType source_type,
               int32_t run_types);
  void OnMenuClosed();
  void RunOverflowMenuClosedCallback();
  void ReleaseRunner();
  void ResetModels();
  void BuildInputContextMenu(const content::ContextMenuParams& params,
                             const std::u16string& clipboard_text);
  void BuildSelectionContextMenu(const content::ContextMenuParams& params);
  void BuildBackgroundContextMenu(const content::ContextMenuParams& params);
  void AppendSettingsContextMenu();
#if BUILDFLAG(IS_WIN)
  void AppendWindowControlsContextMenu();
#endif  // BUILDFLAG(IS_WIN)
  std::u16string GetPasteAndGoMenuLabel(
      const std::u16string& clipboard_text) const;

  views::Widget* widget() { return delegate_->GetContextMenuHostWidget(); }
  const views::Widget* widget() const {
    return delegate_->GetContextMenuHostWidget();
  }
  content::WebContents* web_contents() {
    return delegate_->GetContextMenuWebContents();
  }
  const content::WebContents* web_contents() const {
    return delegate_->GetContextMenuWebContents();
  }
  Profile* profile() { return delegate_->GetContextMenuProfile(); }

  const raw_ref<Delegate> delegate_;
  MenuRunnerFactory menu_runner_factory_;

  bool is_open_ = false;
  content::ContextMenuParams last_params_;
#if BUILDFLAG(IS_MAC)
  std::unique_ptr<ui::SimpleMenuModel> writing_direction_submenu_model_;
#endif  // BUILDFLAG(IS_MAC)
  std::unique_ptr<ui::SimpleMenuModel> context_menu_model_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;
  // Runs when the overflow (three-dot) menu closes. Null unless an overflow
  // menu is currently open.
  base::OnceClosure overflow_menu_closed_callback_;

  base::WeakPtrFactory<OmniboxEverywhereContextMenuController> weak_factory_{
      this};
};

}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_CONTEXT_MENU_CONTROLLER_H_
