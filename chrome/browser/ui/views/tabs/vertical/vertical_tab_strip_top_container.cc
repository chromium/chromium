// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_top_container.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int kTopButtonContainerHeight = 28;
constexpr int kTopButtonPadding = 4;
}  // namespace

namespace {

class TopContainerButton : public views::LabelButton {
  METADATA_HEADER(TopContainerButton, views::LabelButton)
 public:
  TopContainerButton() = default;

  // views::LabelButton:
  std::unique_ptr<views::ActionViewInterface> GetActionViewInterface() override;
};
BEGIN_METADATA(TopContainerButton)
END_METADATA

class TopContainerButtonActionViewInterface
    : public views::LabelButtonActionViewInterface {
 public:
  explicit TopContainerButtonActionViewInterface(
      TopContainerButton* action_view)
      : views::LabelButtonActionViewInterface(action_view),
        action_view_(action_view) {}

  void ActionItemChangedImpl(actions::ActionItem* action_item) override {
    ButtonActionViewInterface::ActionItemChangedImpl(action_item);
    action_view_->SetImageModel(views::Button::STATE_NORMAL,
                                action_item->GetImage());
  }

 private:
  raw_ptr<TopContainerButton> action_view_ = nullptr;
};

std::unique_ptr<views::ActionViewInterface>
TopContainerButton::GetActionViewInterface() {
  return std::make_unique<TopContainerButtonActionViewInterface>(this);
}
}  // namespace

VerticalTabStripTopContainer::VerticalTabStripTopContainer(
    tabs::VerticalTabStripStateController* state_controller,
    actions::ActionItem* root_action_item)
    : state_controller_(state_controller),
      root_action_item_(root_action_item),
      action_view_controller_(std::make_unique<views::ActionViewController>()) {
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));

  tab_search_button_ = AddChildButtonFor(kActionTabSearch);

  collapse_button_ = AddChildButtonFor(kActionToggleCollapseVertical);

  tab_search_button_->SetProperty(views::kElementIdentifierKey,
                                  kTabSearchButtonElementId);

  collapse_button_->SetProperty(views::kElementIdentifierKey,
                                kVerticalTabStripCollapseButtonElementId);

  SetProperty(views::kElementIdentifierKey,
              kVerticalTabStripTopContainerElementId);
}

VerticalTabStripTopContainer::~VerticalTabStripTopContainer() = default;

// TODO(crbug.com/445528000): Update height calculation after child components
// are added
views::ProposedLayout VerticalTabStripTopContainer::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layout;
  if (size_bounds.width().is_bounded()) {
    layout.host_size =
        gfx::Size(size_bounds.width().value(), kTopButtonContainerHeight);
  } else {
    layout.host_size = gfx::Size(parent()->width(), kTopButtonContainerHeight);
  }

  CHECK(tab_search_button_);
  CHECK(collapse_button_);

  const gfx::Size tab_search_button_pref_size =
      tab_search_button_->GetPreferredSize(views::SizeBounds(layout.host_size));
  const gfx::Size collapse_button_pref_size =
      collapse_button_->GetPreferredSize(views::SizeBounds(layout.host_size));

  int current_x = layout.host_size.width();

  // Calculate bounds to right-align the button horizontally and center it
  // vertically within the available space.
  gfx::Rect tab_search_button_bounds(
      current_x - tab_search_button_pref_size.width(),
      (layout.host_size.height() - tab_search_button_pref_size.height()) / 2,
      tab_search_button_pref_size.width(),
      tab_search_button_pref_size.height());
  layout.child_layouts.emplace_back(
      tab_search_button_.get(), tab_search_button_->GetVisible(),
      tab_search_button_bounds, views::SizeBounds(tab_search_button_pref_size));

  current_x = tab_search_button_bounds.x() - kTopButtonPadding;

  // Re-calculate bounds based on new x value, offset by the tab search button.
  gfx::Rect collapse_button_bounds(
      current_x - collapse_button_pref_size.width(),
      (layout.host_size.height() - collapse_button_pref_size.height()) / 2,
      collapse_button_pref_size.width(), collapse_button_pref_size.height());
  layout.child_layouts.emplace_back(
      collapse_button_.get(), collapse_button_->GetVisible(),
      collapse_button_bounds, views::SizeBounds(collapse_button_pref_size));

  return layout;
}

views::LabelButton* VerticalTabStripTopContainer::AddChildButtonFor(
    actions::ActionId action_id) {
  std::unique_ptr<TopContainerButton> label_button =
      std::make_unique<TopContainerButton>();
  actions::ActionItem* action_item =
      actions::ActionManager::Get().FindAction(action_id, root_action_item_);
  CHECK(action_item);

  action_view_controller_->CreateActionViewRelationship(
      label_button.get(), action_item->GetAsWeakPtr());

  TopContainerButton* raw_label_button = AddChildView(std::move(label_button));

  raw_label_button->SetHorizontalAlignment(gfx::ALIGN_RIGHT);

  return raw_label_button;
}

bool VerticalTabStripTopContainer::IsPositionInWindowCaption(
    const gfx::Point& point) {
  const auto get_target_rect = [&](views::View* target) {
    const gfx::Rect& rect = gfx::Rect(point, gfx::Size(1, 1));
    gfx::RectF rect_in_target_coords_f(rect);
    View::ConvertRectToTarget(this, target, &rect_in_target_coords_f);
    return gfx::ToEnclosingRect(rect_in_target_coords_f);
  };

  if (tab_search_button_ && tab_search_button_->GetLocalBounds().Intersects(
                                get_target_rect(tab_search_button_))) {
    return !tab_search_button_->HitTestRect(
        get_target_rect(tab_search_button_));
  }

  if (collapse_button_ && collapse_button_->GetLocalBounds().Intersects(
                              get_target_rect(collapse_button_))) {
    return !collapse_button_->HitTestRect(get_target_rect(collapse_button_));
  }

  return true;
}

BEGIN_METADATA(VerticalTabStripTopContainer)
END_METADATA
