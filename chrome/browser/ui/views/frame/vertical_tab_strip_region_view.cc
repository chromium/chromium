// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_feature.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_bottom_container.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_top_container.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/background.h"
#include "ui/views/controls/resize_area.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {
constexpr gfx::Insets kRegionInteriorMargins = gfx::Insets::VH(8, 0);

constexpr int kRegionVerticalPadding = 5;
}  // namespace

VerticalTabStripRegionView::VerticalTabStripRegionView(
    tabs_api::TabStripService* service_register,
    tabs::VerticalTabStripStateController* state_controller,
    actions::ActionItem* root_action_item,
    BrowserWindowInterface* browser)
    : state_controller_(state_controller) {
  SetBackground(views::CreateSolidBackground(ui::kColorFrameActive));
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetInteriorMargin(kRegionInteriorMargins)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::VH(
                      kRegionVerticalPadding,
                      GetLayoutConstant(VERTICAL_TAB_STRIP_HORIZONTAL_PADDING)))
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kPreferred));

  // Create child views.
  top_button_container_ =
      AddChildView(std::make_unique<VerticalTabStripTopContainer>(
          state_controller_, root_action_item));

  top_button_separator_ = AddChildView(std::make_unique<views::Separator>());
  top_button_separator_->SetColorId(kColorTabDividerFrameActive);

  bottom_button_container_ =
      AddChildView(std::make_unique<VerticalTabStripBottomContainer>(
          state_controller_, root_action_item, browser));

  gemini_button_ = AddChildView(std::make_unique<views::View>());

  resize_area_ = AddChildView(std::make_unique<views::ResizeArea>(this));
  resize_area_->SetProperty(views::kViewIgnoredByLayoutKey, true);

  collapsed_state_changed_subscription_ =
      state_controller_->RegisterOnStateChanged(base::BindRepeating(
          &VerticalTabStripRegionView::OnCollapsedStateChanged,
          base::Unretained(this)));

  SetProperty(views::kElementIdentifierKey, kVerticalTabStripRegionElementId);

  root_node_ = std::make_unique<RootTabCollectionNode>(
      service_register,
      base::BindRepeating(&VerticalTabStripRegionView::SetTabStripView,
                          base::Unretained(this)));
}

VerticalTabStripRegionView::~VerticalTabStripRegionView() = default;

void VerticalTabStripRegionView::Layout(PassKey) {
  LayoutSuperclass<views::AccessiblePaneView>(this);

  // Manually position the resize area as it overlaps views handled by the flex
  // layout.
  resize_area_->SetBoundsRect(gfx::Rect(bounds().right() - kResizeAreaWidth, 0,
                                        kResizeAreaWidth, bounds().height()));
}

void VerticalTabStripRegionView::OnResize(int resize_amount,
                                          bool done_resizing) {}

bool VerticalTabStripRegionView::IsPositionInWindowCaption(
    const gfx::Point& point) {
  if (GetTopContainer()->bounds().Contains(point)) {
    return GetTopContainer()->IsPositionInWindowCaption(point);
  }

  return false;
}

views::View* VerticalTabStripRegionView::SetTabStripView(
    std::unique_ptr<views::View> view) {
  CHECK(views::IsViewClass<VerticalTabStripView>(view.get()));
  tab_strip_view_ =
      static_cast<VerticalTabStripView*>(AddChildView(std::move(view)));
  tab_strip_view_->SetCollapsedState(state_controller_->IsCollapsed());
  gfx::Insets tab_container_margins = gfx::Insets::TLBR(
      kRegionVerticalPadding,
      GetLayoutConstant(VERTICAL_TAB_STRIP_HORIZONTAL_PADDING),
      kRegionVerticalPadding, 0);
  tab_strip_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  tab_strip_view_->SetProperty(views::kMarginsKey, tab_container_margins);
  std::optional<size_t> separator_index = GetIndexOf(top_button_separator_);
  CHECK(separator_index.has_value());
  ReorderChildView(tab_strip_view_, separator_index.value() + 1);
  return tab_strip_view_;
}

void VerticalTabStripRegionView::OnCollapsedStateChanged(
    tabs::VerticalTabStripStateController* state_controller) {
  tab_strip_view_->SetCollapsedState(state_controller->IsCollapsed());
  bottom_button_container_->OnCollapsedStateChanged(state_controller);
}

BEGIN_METADATA(VerticalTabStripRegionView)
END_METADATA
