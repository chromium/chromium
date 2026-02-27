// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_SHARED_TAB_STRIP_COMBO_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_SHARED_TAB_STRIP_COMBO_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/tab_search_bubble_host_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"

namespace tab_groups {
class STGEverythingMenu;
}  // namespace tab_groups

namespace views {
class ActionViewController;
class MenuButtonController;
class MenuModelAdapter;
class MenuRunner;
}  // namespace views

class BrowserWindowInterface;
class TabSearchBubbleHost;
class TabStripFlatEdgeButton;

// A container for two TabStripFlatEdgeButtons that manages their flat edges
// based on visibility and the combo button's orientation.
class TabStripComboButton : public views::View,
                            public views::ContextMenuController,
                            public ui::SimpleMenuModel::Delegate,
                            public TabSearchBubbleHostObserver,
                            public gfx::AnimationDelegate {
  METADATA_HEADER(TabStripComboButton, views::View)
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTabSearchUnpinMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kProjectsPanelUnpinMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kEverythingMenuUnpinMenuItem);

  explicit TabStripComboButton(BrowserWindowInterface* browser);
  TabStripComboButton(const TabStripComboButton&) = delete;
  TabStripComboButton& operator=(const TabStripComboButton&) = delete;
  ~TabStripComboButton() override;

  void SetOrientation(views::LayoutOrientation orientation);

  TabStripFlatEdgeButton* start_button() { return start_button_; }
  TabStripFlatEdgeButton* end_button() { return end_button_; }

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

  // TabSearchBubbleHostObserver:
  void OnBubbleInitializing() override;
  void OnBubbleDestroying() override;
  void OnHostDestroying() override;

  void SetTabSearchBubbleHost(TabSearchBubbleHost* host);

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  gfx::Size GetPreferredSizeForOrientation(
      views::LayoutOrientation orientation);

 protected:
  // views::View:
  void ChildVisibilityChanged(views::View* child) override;

 private:
  void ShowEverythingMenu();

  void UpdateButtonsVisibility();

  std::unique_ptr<TabStripFlatEdgeButton> CreateFlatEdgeButtonFor(
      actions::ActionId action_id,
      ui::ElementIdentifier element_id);

  void UpdateStyles();

  void OnMenuClosed();

  void MaybeHideTabSearchButton();

  actions::ActionItem* GetStartButtonActionItem();
  actions::ActionItem* GetEndButtonActionItem();

  const raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<TabStripFlatEdgeButton> start_button_ = nullptr;
  raw_ptr<TabStripFlatEdgeButton> end_button_ = nullptr;

  gfx::SlideAnimation start_button_animation_{this};
  gfx::SlideAnimation end_button_animation_{this};

  views::LayoutOrientation orientation_ = views::LayoutOrientation::kHorizontal;

  bool show_tab_search_ephemerally_ = false;

  PrefChangeRegistrar pref_registrar_;

  std::unique_ptr<views::ActionViewController> action_view_controller_;
  std::unique_ptr<tab_groups::STGEverythingMenu> everything_menu_;
  raw_ptr<views::MenuButtonController> everything_menu_controller_ = nullptr;

  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuModelAdapter> menu_model_adapter_;
  std::unique_ptr<views::MenuRunner> menu_runner_;

  base::OneShotTimer hide_tab_search_timer_;
  base::ScopedObservation<TabSearchBubbleHost, TabSearchBubbleHostObserver>
      tab_search_bubble_host_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_SHARED_TAB_STRIP_COMBO_BUTTON_H_
