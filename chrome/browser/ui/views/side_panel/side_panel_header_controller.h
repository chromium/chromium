// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_HEADER_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_HEADER_CONTROLLER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_header.h"
#include "chrome/browser/ui/views/side_panel/side_panel_toolbar_pinning_controller.h"

class Browser;
class SidePanelEntry;

namespace ui {
class ImageModel;
class MenuModel;
}  // namespace ui

namespace views {
class ImageButton;
class ImageView;
class Label;
class MenuRunner;
class ToggleImageButton;
}  // namespace views

class SidePanelHeaderController
    : public SidePanelHeader::Delegate,
      public SidePanelToolbarPinningController::Observer {
 public:
  SidePanelHeaderController(
      Browser* browser,
      SidePanelToolbarPinningController* side_panel_toolbar_pinning_controller,
      SidePanelEntry* side_panel_entry);
  ~SidePanelHeaderController() override;

  // SidePanelHeaderDelegate:
  std::unique_ptr<views::ImageView> CreatePanelIcon() override;
  std::unique_ptr<views::Label> CreatePanelTitle() override;
  std::unique_ptr<views::ToggleImageButton> CreatePinButton() override;
  std::unique_ptr<views::ImageButton> CreateOpenNewTabButton() override;
  std::unique_ptr<views::ImageButton> CreateMoreInfoButton() override;
  std::unique_ptr<views::ImageButton> CreateCloseButton() override;

  // SidePanelToolbarPinningController::Observer:
  void OnPinStateChanged() override;

 private:
  void OnActionItemChanged();
  void UpdateSidePanelHeader();
  void UpdatePinButton();
  ui::ImageModel GetIconImage();
  std::u16string_view GetTitleText();

  // Button click callbacks:
  void UpdatePinState();
  void OpenInNewTab();
  void OpenMoreInfoMenu();
  void Close();

  void MaybeQueuePinPromo(SidePanelEntryId id);
  void ShowPinPromo();
  void MaybeEndPinPromo(bool pinned);

  raw_ptr<Browser> browser_ = nullptr;
  raw_ptr<SidePanelToolbarPinningController>
      side_panel_toolbar_pinning_controller_ = nullptr;
  base::WeakPtr<SidePanelEntry> side_panel_entry_;

  raw_ptr<views::ImageView> panel_icon_ = nullptr;
  raw_ptr<views::Label> panel_title_ = nullptr;
  raw_ptr<views::ImageButton> open_new_tab_button_ = nullptr;
  raw_ptr<views::ToggleImageButton> pin_button_ = nullptr;
  raw_ptr<views::ImageButton> more_info_button_ = nullptr;
  raw_ptr<views::ImageButton> close_button_ = nullptr;

  // This subscription is used to update the side panel title when the action
  // item associated with the side panel entry changes.
  base::CallbackListSubscription action_item_controller_subscription_;

  base::ScopedObservation<SidePanelToolbarPinningController,
                          SidePanelHeaderController::Observer>
      side_panel_toolbar_pinning_controller_observation_{this};

  // Model for the more info menu.
  std::unique_ptr<ui::MenuModel> more_info_menu_model_;
  // Runner for the more info menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // Provides delay on pinning promo.
  base::OneShotTimer pin_promo_timer_;
  // Set to the appropriate pin promo for the current side panel entry, or null
  // if none. (Not set if e.g. already pinned.)
  raw_ptr<const base::Feature> pending_pin_promo_ = nullptr;

  base::WeakPtrFactory<SidePanelHeaderController> weak_pointer_factor_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_HEADER_CONTROLLER_H_
