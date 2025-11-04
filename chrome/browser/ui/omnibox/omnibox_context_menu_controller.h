// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTEXT_MENU_CONTROLLER_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTEXT_MENU_CONTROLLER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "ui/menus/simple_menu_model.h"

class BrowserWindowInterface;
class GURL;
class FaviconService;

namespace favicon_base {
struct FaviconImageResult;
}  // namespace favicon_base

namespace ui {
class ImageModel;
}  // namespace ui

// OmniboxContextMenuController creates and manages state for the context menu
// shown for the omnibox.
class OmniboxContextMenuController : public ui::SimpleMenuModel::Delegate {
 public:
  explicit OmniboxContextMenuController(
      BrowserWindowInterface* browser_window_interface);

  OmniboxContextMenuController(const OmniboxContextMenuController&) = delete;
  OmniboxContextMenuController& operator=(const OmniboxContextMenuController&) =
      delete;

  ~OmniboxContextMenuController() override;

  ui::SimpleMenuModel* menu_model() { return menu_model_.get(); }

  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  void BuildMenu();
  // Adds a IDC_* style command to the menu with a string16.
  void AddItem(int id, const std::u16string str);
  // Adds a IDC_* style command to the menu with a localized string and icon.
  void AddItemWithStringIdAndIcon(int id,
                                  int localization_id,
                                  const ui::ImageModel& icon);
  // Adds a IDC_* style command to the menu with a string16 and icon.
  void AddItemWithIcon(int command_id,
                       const std::u16string& label,
                       const ui::ImageModel& icon);
  // Adds a separator to the menu.
  void AddSeparator();
  // Adds recent tabs as items to the menu.
  void AddRecentTabItems();
  // Adds the static items with icons.
  void AddStaticItems();
  // Adds a title with a localized string to the menu.
  void AddTitleWithStringId(int localization_id);
  // Adds the tabs favicon to the menu.
  void AddTabFavicon(int command_id,
                     const GURL& url,
                     const std::u16string& label);
  // Callback for when the tab favicon is available.
  void OnFaviconDataAvailable(
      int command_id,
      const favicon_base::FaviconImageResult& image_result);
  // Returns whether the tab is valid to be shown in the context menu.
  bool IsValidTab(GURL url);

  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  raw_ptr<BrowserWindowInterface> browser_window_interface_;
  // Needed for using FaviconService.
  base::CancelableTaskTracker cancelable_task_tracker_;
  raw_ptr<FaviconService> favicon_service_;
  int next_command_id_ = 0;
  base::WeakPtrFactory<OmniboxContextMenuController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTEXT_MENU_CONTROLLER_H_
