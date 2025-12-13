// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_CONTEXT_MENU_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/omnibox/omnibox_context_menu_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "ui/base/models/menu_model_delegate.h"
#include "ui/views/controls/menu/menu_delegate.h"

namespace views {
class MenuItemView;
class MenuRunner;
class Widget;
}  // namespace views

namespace content {
class WebContents;
}  // namespace content

class OmniboxPopupFileSelector;

class OmniboxContextMenu : public views::MenuDelegate,
                           public ui::MenuModelDelegate {
 public:
  explicit OmniboxContextMenu(
      views::Widget* parent_widget,
      OmniboxPopupFileSelector* file_selector,
      content::WebContents* web_contents,
      base::RepeatingClosure on_menu_closed = base::RepeatingClosure());

  ~OmniboxContextMenu() override;

  views::MenuItemView* menu() const { return menu_; }

  // Shows the context menu at the specified point.
  void RunMenuAt(const gfx::Point& point,
                 ui::mojom::MenuSourceType source_type);

  // views::MenuDelegate:
  void ExecuteCommand(int command_id, int event_flags) override;
  const gfx::FontList* GetLabelFontList(int command_id) const override;
  std::optional<SkColor> GetLabelColor(int command_id) const override;
  int GetMaxWidthForMenu(views::MenuItemView* menu) override;
  bool IsCommandEnabled(int command_id) const override;
  bool IsCommandVisible(int command_id) const override;
  void OnMenuClosed(views::MenuItemView* menu) override;

  // ui::MenuModelDelegate:
  void OnIconChanged(int command_id) override;

 private:
  const raw_ptr<views::Widget> parent_widget_;
  std::unique_ptr<OmniboxContextMenuController> controller_;

  // Responsible for running the menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;
  // The menu itself. This is owned by `menu_runner_`.
  raw_ptr<views::MenuItemView> menu_;
  // Optional callback to run after ExecuteCommand is called.
  base::RepeatingClosure on_menu_closed_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_CONTEXT_MENU_H_
