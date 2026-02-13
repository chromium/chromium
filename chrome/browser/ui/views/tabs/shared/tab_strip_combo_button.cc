// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/shared/tab_strip_combo_button.h"

#include "base/i18n/rtl.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_everything_menu.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_flat_edge_button.h"
#include "components/saved_tab_groups/public/features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

TabStripComboButton::TabStripComboButton(BrowserWindowInterface* browser)
    : browser_(browser),
      action_view_controller_(std::make_unique<views::ActionViewController>()) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      GetLayoutConstant(
          LayoutConstant::kVerticalTabStripFlatEdgeButtonPadding)));

  std::unique_ptr<TabStripFlatEdgeButton> start_button;
  if (tab_groups::IsProjectsPanelFeatureEnabled()) {
    start_button = CreateFlatEdgeButtonFor(
        kActionToggleProjectsPanel, kVerticalTabStripProjectsButtonElementId);
  } else if (tab_groups::SavedTabGroupUtils::IsEnabledForProfile(
                 browser_->GetProfile())) {
    start_button = CreateFlatEdgeButtonFor(kActionTabGroupsMenu,
                                           kSavedTabGroupButtonElementId);

    auto controller = std::make_unique<views::MenuButtonController>(
        start_button.get(),
        base::BindRepeating(&TabStripComboButton::ShowEverythingMenu,
                            base::Unretained(this)),
        std::make_unique<views::Button::DefaultButtonControllerDelegate>(
            start_button.get()));
    everything_menu_controller_ = controller.get();
    start_button->SetButtonController(std::move(controller));
  }

  if (start_button) {
    start_button_ = AddChildView(std::move(start_button));
  }

  end_button_ = AddChildView(
      CreateFlatEdgeButtonFor(kActionTabSearch, kTabSearchButtonElementId));

  UpdateStyles();
}

TabStripComboButton::~TabStripComboButton() = default;

void TabStripComboButton::SetOrientation(views::LayoutOrientation orientation) {
  if (orientation_ == orientation) {
    return;
  }
  orientation_ = orientation;

  views::BoxLayout* layout = static_cast<views::BoxLayout*>(GetLayoutManager());
  layout->SetOrientation(orientation_);
  layout->set_between_child_spacing(GetLayoutConstant(
      LayoutConstant::kVerticalTabStripFlatEdgeButtonPadding));

  UpdateStyles();
}

void TabStripComboButton::ChildVisibilityChanged(views::View* child) {
  UpdateStyles();
}

void TabStripComboButton::ShowEverythingMenu() {
  base::RecordAction(base::UserMetricsAction(
      BrowserView::GetBrowserViewForBrowser(browser_)
              ->ShouldDrawVerticalTabStrip()
          ? "TabGroups_SavedTabGroups_EverythingButtonPressed_Vertical"
          : "TabGroups_SavedTabGroups_EverythingButtonPressed_Horizontal"));
  if (everything_menu_ && everything_menu_->IsShowing()) {
    return;
  }

  everything_menu_ = std::make_unique<tab_groups::STGEverythingMenu>(
      everything_menu_controller_, browser_->GetBrowserForMigrationOnly(),
      tab_groups::STGEverythingMenu::MenuContext::kVerticalTabStrip);

  everything_menu_->RunMenu();
}

std::unique_ptr<TabStripFlatEdgeButton>
TabStripComboButton::CreateFlatEdgeButtonFor(actions::ActionId action_id,
                                             ui::ElementIdentifier element_id) {
  auto button = std::make_unique<TabStripFlatEdgeButton>();
  if (!browser_ || !browser_->GetActions()) {
    return button;
  }
  actions::ActionItem* action_item = actions::ActionManager::Get().FindAction(
      action_id, browser_->GetActions()->root_action_item());
  CHECK(action_item);
  action_view_controller_->CreateActionViewRelationship(
      button.get(), action_item->GetAsWeakPtr());
  button->SetProperty(views::kElementIdentifierKey, element_id);

  const int raw_button_size = GetLayoutConstant(
      LayoutConstant::kVerticalTabStripTopContainerButtonSize);
  button->SetPreferredSize(gfx::Size(raw_button_size, raw_button_size));

  return button;
}

void TabStripComboButton::UpdateStyles() {
  const bool both_visible = start_button_ && start_button_->GetVisible() &&
                            end_button_ && end_button_->GetVisible();
  const bool is_vertical = orientation_ == views::LayoutOrientation::kVertical;

  if (start_button_) {
    TabStripFlatEdgeButton::FlatEdge flat_edge =
        TabStripFlatEdgeButton::FlatEdge::kNone;
    if (both_visible) {
      if (is_vertical) {
        flat_edge = TabStripFlatEdgeButton::FlatEdge::kBottom;
      } else {
        flat_edge = base::i18n::IsRTL()
                        ? TabStripFlatEdgeButton::FlatEdge::kLeft
                        : TabStripFlatEdgeButton::FlatEdge::kRight;
      }
    }
    start_button_->SetFlatEdge(flat_edge);
  }

  if (end_button_) {
    TabStripFlatEdgeButton::FlatEdge flat_edge =
        TabStripFlatEdgeButton::FlatEdge::kNone;
    if (both_visible) {
      if (is_vertical) {
        flat_edge = TabStripFlatEdgeButton::FlatEdge::kTop;
      } else {
        flat_edge = base::i18n::IsRTL()
                        ? TabStripFlatEdgeButton::FlatEdge::kRight
                        : TabStripFlatEdgeButton::FlatEdge::kLeft;
      }
    }
    end_button_->SetFlatEdge(flat_edge);
  }
}

BEGIN_METADATA(TabStripComboButton)
END_METADATA
