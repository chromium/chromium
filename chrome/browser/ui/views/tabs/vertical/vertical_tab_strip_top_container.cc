// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_top_container.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_everything_menu.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_combo_button.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_flat_edge_button.h"
#include "chrome/browser/ui/views/tabs/vertical/top_container_button.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view_class_properties.h"

VerticalTabStripTopContainer::VerticalTabStripTopContainer(
    tabs::VerticalTabStripStateController* state_controller,
    actions::ActionItem* root_action_item,
    BrowserWindowInterface* browser)
    : state_controller_(state_controller),
      root_action_item_(root_action_item),
      browser_(browser),
      action_view_controller_(std::make_unique<views::ActionViewController>()) {
  SetProperty(views::kElementIdentifierKey,
              kVerticalTabStripTopContainerElementId);
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));

  collapsed_state_changed_subscription_ =
      state_controller->RegisterOnCollapseChanged(base::BindRepeating(
          &VerticalTabStripTopContainer::OnCollapsedStateChanged,
          base::Unretained(this)));

  collapse_button_ =
      AddTopContainerChildButtonFor(kActionToggleCollapseVertical);
  collapse_button_->SetProperty(views::kElementIdentifierKey,
                                kVerticalTabStripCollapseButtonElementId);

  std::unique_ptr<TabStripFlatEdgeButton> tab_group_button;
  if (tabs::IsProjectsPanelFeatureEnabled()) {
    tab_group_button = CreateFlatEdgeButtonFor(kActionToggleProjectsPanel);
    tab_group_button->SetProperty(views::kElementIdentifierKey,
                                  kVerticalTabStripProjectsButtonElementId);
  } else if (tab_groups::SavedTabGroupUtils::IsEnabledForProfile(
                 browser_->GetProfile())) {
    tab_group_button = CreateFlatEdgeButtonFor(kActionTabGroupsMenu);
    // Creating MenuButtonController because tab_group_button is a LabelButton.
    auto controller = std::make_unique<views::MenuButtonController>(
        tab_group_button.get(),
        base::BindRepeating(&VerticalTabStripTopContainer::ShowEverythingMenu,
                            base::Unretained(this)),
        std::make_unique<views::Button::DefaultButtonControllerDelegate>(
            tab_group_button.get()));
    everything_menu_controller_ = controller.get();

    tab_group_button->SetButtonController(std::move(controller));
    tab_group_button->SetProperty(views::kElementIdentifierKey,
                                  kSavedTabGroupButtonElementId);
  }

  auto tab_search_button = CreateFlatEdgeButtonFor(kActionTabSearch);
  tab_search_button->SetProperty(views::kElementIdentifierKey,
                                 kTabSearchButtonElementId);

  combo_button_ = AddChildView(std::make_unique<TabStripComboButton>(
      std::move(tab_group_button), std::move(tab_search_button)));
  combo_button_->SetOrientation(state_controller->IsCollapsed()
                                    ? views::LayoutOrientation::kVertical
                                    : views::LayoutOrientation::kHorizontal);
}

VerticalTabStripTopContainer::~VerticalTabStripTopContainer() = default;

views::ProposedLayout VerticalTabStripTopContainer::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layout;
  gfx::Size host_size =
      gfx::Size(size_bounds.width().is_bounded() ? size_bounds.width().value()
                                                 : parent()->width(),
                toolbar_height_);

  std::vector<views::View*> container_views;

  CHECK(combo_button_);
  container_views.push_back(combo_button_);

  CHECK(collapse_button_);
  container_views.push_back(collapse_button_);

  const int padding =
      GetLayoutConstant(LayoutConstant::kVerticalTabStripTopButtonPadding);

  if (state_controller_->IsCollapsed()) {
    int current_y = 0;

    if (collapse_button_) {
      const gfx::Size pref_size = collapse_button_->GetPreferredSize();
      gfx::Rect bounds(std::max(0, (host_size.width() - pref_size.width()) / 2),
                       current_y, pref_size.width(), pref_size.height());
      layout.child_layouts.emplace_back(collapse_button_.get(),
                                        collapse_button_->GetVisible(), bounds);
      host_size.SetToMax(gfx::Size(bounds.right(), 0));

      current_y +=
          pref_size.height() +
          GetLayoutConstant(LayoutConstant::kVerticalTabStripCollapsedPadding);
    }

    if (combo_button_) {
      const gfx::Size pref_size = combo_button_->GetPreferredSize();
      gfx::Rect bounds(std::max(0, (host_size.width() - pref_size.width()) / 2),
                       current_y, pref_size.width(), pref_size.height());
      layout.child_layouts.emplace_back(combo_button_.get(),
                                        combo_button_->GetVisible(), bounds);
      host_size.SetToMax(gfx::Size(bounds.right(), 0));

      current_y += pref_size.height() +
                   GetLayoutConstant(
                       LayoutConstant::kVerticalTabStripFlatEdgeButtonPadding);
    }

    host_size.SetToMax(gfx::Size(0, current_y));
  } else {
    // If the vertical tab strip is uncollapsed, then lay out the buttons
    // horizontally. The exact y-level of the buttons depends on if they can lay
    // on one line or not.
    int total_width = caption_button_width_;
    int min_height = 0;
    for (views::View* container_view : container_views) {
      const auto preferred = container_view->GetPreferredSize();
      total_width += preferred.width();
      min_height = std::max(min_height, preferred.height());
    }

    // Guarantee that the height of the container is at least the height of the
    // buttons plus padding. Use the same padding as the toolbar for approximate
    // consistency.
    if (toolbar_height_ == 0) {
      min_height += GetLayoutInsets(TOOLBAR_BUTTON).height();
    }
    host_size.SetToMax(gfx::Size(0, min_height));

    total_width += (container_views.size() - 1) * padding;

    // If we're trying to get the minimum size, it will ask for layout for size
    // bounds {0, 0}, but overflow is based on available size.
    const int available_width =
        host_size.width() > 0
            ? host_size.width()
            : parent()->GetAvailableSize(this).width().value_or(0);

    // If there is not enough space for the buttons on a single line with
    // caption buttons, shift them below.
    const bool wrapped_due_to_overflow = size_bounds.width().is_bounded() &&
                                         caption_button_width_ > 0 &&
                                         total_width > available_width;

    int y_baseline = host_size.height() / 2;
    // If there is not enough space for all of the buttons to be on the same
    // line as the caption buttons, then we lay them out with collapse_button_
    // anchored to the left. tab_search_ and tab_groups_ are on the right.
    if (wrapped_due_to_overflow) {
      host_size.Enlarge(0,
                        GetLayoutConstant(LayoutConstant::kBookmarkBarHeight));
      y_baseline = toolbar_height_ +
                   (GetLayoutConstant(LayoutConstant::kBookmarkBarHeight) -
                    GetLayoutConstant(
                        LayoutConstant::kBookmarkBarButtonImageLabelPadding)) /
                       2;
    }

    if (collapse_button_) {
      const gfx::Size pref_size = collapse_button_->GetPreferredSize();
      gfx::Rect bounds(wrapped_due_to_overflow ? 0 : caption_button_width_,
                       std::max(0, y_baseline - pref_size.height() / 2),
                       pref_size.width(), pref_size.height());
      layout.child_layouts.emplace_back(collapse_button_.get(),
                                        collapse_button_->GetVisible(), bounds);
    }

    int right_alignment = host_size.width();

    if (combo_button_) {
      const gfx::Size pref_size = combo_button_->GetPreferredSize();
      right_alignment -= pref_size.width();
      gfx::Rect bounds(right_alignment,
                       std::max(0, y_baseline - pref_size.height() / 2),
                       pref_size.width(), pref_size.height());
      layout.child_layouts.emplace_back(combo_button_.get(),
                                        combo_button_->GetVisible(), bounds);

      right_alignment -= GetLayoutConstant(
          LayoutConstant::kVerticalTabStripFlatEdgeButtonPadding);
    }
  }

  layout.host_size = host_size;

  return layout;
}

views::LabelButton* VerticalTabStripTopContainer::AddTopContainerChildButtonFor(
    actions::ActionId action_id) {
  std::unique_ptr<TopContainerButton> container_button =
      std::make_unique<TopContainerButton>();
  actions::ActionItem* action_item =
      actions::ActionManager::Get().FindAction(action_id, root_action_item_);
  CHECK(action_item);

  action_view_controller_->CreateActionViewRelationship(
      container_button.get(), action_item->GetAsWeakPtr());

  TopContainerButton* container_button_ptr =
      AddChildView(std::move(container_button));

  return container_button_ptr;
}

std::unique_ptr<TabStripFlatEdgeButton>
VerticalTabStripTopContainer::CreateFlatEdgeButtonFor(
    actions::ActionId action_id) {
  std::unique_ptr<TabStripFlatEdgeButton> container_button =
      std::make_unique<TabStripFlatEdgeButton>();
  actions::ActionItem* action_item =
      actions::ActionManager::Get().FindAction(action_id, root_action_item_);
  CHECK(action_item);

  action_view_controller_->CreateActionViewRelationship(
      container_button.get(), action_item->GetAsWeakPtr());

  const int raw_container_button_size = GetLayoutConstant(
      LayoutConstant::kVerticalTabStripTopContainerButtonSize);
  container_button->SetPreferredSize(
      gfx::Size(raw_container_button_size, raw_container_button_size));

  return container_button;
}

TabStripComboButton* VerticalTabStripTopContainer::GetComboButton() {
  return combo_button_.get();
}

TabStripFlatEdgeButton* VerticalTabStripTopContainer::GetTabSearchButton() {
  return combo_button_->end_button();
}

bool VerticalTabStripTopContainer::IsPositionInWindowCaption(
    const gfx::Point& point) {
  if (combo_button_ && IsHitInView(combo_button_, point)) {
    return false;
  }

  if (collapse_button_ && IsHitInView(collapse_button_, point)) {
    return false;
  }

  return true;
}

void VerticalTabStripTopContainer::SetToolbarHeightForLayout(
    int toolbar_height) {
  if (toolbar_height_ == toolbar_height) {
    return;
  }
  toolbar_height_ = toolbar_height;
  InvalidateLayout();
}

void VerticalTabStripTopContainer::SetCaptionButtonWidthForLayout(
    int caption_button_width) {
  if (caption_button_width_ == caption_button_width) {
    return;
  }
  caption_button_width_ = caption_button_width;
  InvalidateLayout();
}

void VerticalTabStripTopContainer::ShowEverythingMenu() {
  if (everything_menu_ && everything_menu_->IsShowing()) {
    return;
  }

  // Creating everything menu.
  everything_menu_ = std::make_unique<tab_groups::STGEverythingMenu>(
      everything_menu_controller_, browser_->GetBrowserForMigrationOnly(),
      tab_groups::STGEverythingMenu::MenuContext::kVerticalTabStrip);

  everything_menu_->RunMenu();
}

void VerticalTabStripTopContainer::OnCollapsedStateChanged(
    tabs::VerticalTabStripStateController* controller) {
  if (combo_button_) {
    combo_button_->SetOrientation(controller->IsCollapsed()
                                      ? views::LayoutOrientation::kVertical
                                      : views::LayoutOrientation::kHorizontal);
  }
}

BEGIN_METADATA(VerticalTabStripTopContainer)
END_METADATA
