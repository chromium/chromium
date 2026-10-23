// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_top_container.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/organizer/layout_constants.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_combo_button.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_flat_edge_button.h"
#include "chrome/browser/ui/views/tabs/vertical/top_container_button.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_segmented_control.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view_class_properties.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(
    VerticalTabStripTopContainer,
    kToggleVerticalTabsExpandOnHoverElementId);

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

  collapse_button_ = AddChildButtonFor(kActionToggleCollapseVertical);
  collapse_button_->set_context_menu_controller(this);
  collapse_button_->SetProperty(views::kElementIdentifierKey,
                                kVerticalTabStripCollapseButtonElementId);

  combo_button_ = AddChildView(std::make_unique<TabStripComboButton>(
      browser_, TabStripComboButton::Context::kVerticalTabStrip));
  combo_button_->SetPaintToLayer();
  combo_button_->layer()->SetFillsBoundsOpaquely(false);
  combo_button_->SetClipLayerToVisibleBounds(true);
  combo_button_->SetOrientation(
      combo_button_orientation_ = state_controller->IsCollapsed()
                                      ? views::LayoutOrientation::kVertical
                                      : views::LayoutOrientation::kHorizontal);

  if (organizer_panel::IsOrganizerPanelFeatureEnabled()) {
    if (organizer_panel::IsOrganizerPanelSegmentationControlEnabled()) {
      segmented_control_ = AddChildView(
          std::make_unique<VerticalTabStripSegmentedControl>(browser_));
      segmented_control_->SetVisible(false);
    }

    auto invalidate_layout = base::BindRepeating(
        &VerticalTabStripTopContainer::InvalidateLayout, base::Unretained(this),
        /*avoid_propagate_during_layout=*/false);

    if (browser_ && browser_->GetProfile()) {
      pref_registrar_.Init(browser_->GetProfile()->GetPrefs());
      pref_registrar_.Add(prefs::kTabSearchPinnedToTabstrip, invalidate_layout);
    }

    if (state_controller_) {
      expand_on_hover_subscription_ =
          state_controller_->RegisterOnExpandOnHoverEnabledChanged(
              base::IgnoreArgs<bool>(invalidate_layout));
      mode_changed_subscription_ = state_controller_->RegisterOnModeChanged(
          base::IgnoreArgs<tabs::VerticalTabStripStateController*>(
              invalidate_layout));
    }
  }
}

VerticalTabStripTopContainer::~VerticalTabStripTopContainer() = default;

void VerticalTabStripTopContainer::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);
  combo_button_->SetOrientation(combo_button_orientation_);
}

views::ProposedLayout VerticalTabStripTopContainer::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layout;
  gfx::Size host_size =
      gfx::Size(size_bounds.width().is_bounded() ? size_bounds.width().value()
                                                 : parent()->width(),
                toolbar_height_);

  // If we're trying to get the minimum size, it will ask for layout for size
  // bounds {0, 0}, but overflow is based on available size.
  const int available_width =
      host_size.width() > 0
          ? host_size.width()
          : parent()->GetAvailableSize(this).width().value_or(0);

  const bool is_collapsed =
      state_controller_ && state_controller_->IsCollapsed();
  const bool is_collapsing =
      state_controller_ && state_controller_->GetCollapseState() ==
                               tabs::VerticalTabStripCollapseState::kCollapsing;
  const int uncollapsed_width =
      state_controller_ ? state_controller_->GetUncollapsedWidth() : 0;
  const bool is_expand_on_hover =
      state_controller_ && state_controller_->IsExpandOnHoverEnabled();
  const bool keep_horizontal_layout =
      (is_collapsed || is_collapsing) && is_expand_on_hover &&
      organizer_panel::IsOrganizerPanelFeatureEnabled();
  const bool show_segmented_control =
      ShouldShowSegmentedControl(available_width);
  const int trailing_control_width =
      GetTrailingControlPreferredSize(show_segmented_control).width();

  // Once the available width is below the collapse snap width, we update the
  // orientation in order to have smooth transition during animation.
  if (!keep_horizontal_layout &&
      (trailing_control_width >= available_width ||
       available_width < tabs::kVerticalTabStripCollapseSnapWidth ||
       is_collapsed)) {
    combo_button_orientation_ = views::LayoutOrientation::kVertical;
    int current_y = 0;

    if (segmented_control_) {
      layout.child_layouts.emplace_back(segmented_control_.get(),
                                        /*visible=*/false);
    }

    if (collapse_button_) {
      const gfx::Size pref_size = collapse_button_->GetPreferredSize();
      const int baseline_height =
          GetBaselineMinHeight(/*show_segmented_control=*/false);
      current_y = std::max(0, (baseline_height - pref_size.height()) / 2);

      // Because the collapsed state is set at the end of the transition, we
      // need to ensure that the top container's button do not appear behind the
      // caption buttons when they are leading.
      if (caption_button_width_ > 0) {
        current_y = std::max(current_y, toolbar_height_);
      }

      int x = is_collapsed
                  ? 0
                  : std::max(0, (host_size.width() - pref_size.width()) / 2);
      gfx::Rect bounds(x, current_y, pref_size.width(), pref_size.height());
      layout.child_layouts.emplace_back(collapse_button_.get(),
                                        /*visible=*/true, bounds);
      host_size.SetToMax(gfx::Size(bounds.right(), 0));

      current_y += pref_size.height();
    }

    if (combo_button_) {
      // In the case that neither of the combo button components are visible, we
      // do not want to add any extra padding to the top container.
      bool start_button_visible = combo_button_->start_button() &&
                                  combo_button_->start_button()->GetVisible();
      bool end_button_visible = combo_button_->end_button() &&
                                combo_button_->end_button()->GetVisible();
      if (start_button_visible || end_button_visible) {
        current_y += GetLayoutConstant(
            LayoutConstant::kVerticalTabStripCollapsedVerticalPadding);

        const gfx::Size pref_size =
            combo_button_->GetPreferredSizeForOrientation(
                combo_button_orientation_);

        const int kComboButtonCollapsedPadding = GetLayoutConstant(
            LayoutConstant::kVerticalTabStripFlatEdgeButtonPadding);

        // When collapsed the combo button should fill the width (excluding
        // padding), this is especially relevant when in the expand-on-hover
        // state.
        int width =
            is_collapsed
                ? (host_size.width() - (2 * kComboButtonCollapsedPadding))
                : pref_size.width();
        int padding = is_collapsed
                          ? kComboButtonCollapsedPadding
                          : std::max(0, (host_size.width() - width) / 2);
        gfx::Rect bounds(padding, current_y, width, pref_size.height());
        layout.child_layouts.emplace_back(combo_button_.get(),
                                          /*visible=*/true, bounds);
        host_size.SetToMax(gfx::Size(bounds.right(), 0));

        current_y +=
            pref_size.height() +
            GetLayoutConstant(
                LayoutConstant::kVerticalTabStripCollapsedVerticalPadding);
      } else {
        layout.child_layouts.emplace_back(combo_button_.get(),
                                          /*visible=*/true);
        if (caption_button_width_ != 0) {
          current_y += GetLayoutConstant(
              LayoutConstant::kVerticalTabStripCollapsedVerticalPadding);
        }
      }
    }

    host_size.SetToMax(gfx::Size(0, current_y));
  } else {
    // If the vertical tab strip is uncollapsed, then lay out the buttons
    // horizontally. The exact y-level of the buttons depends on if they can lay
    // on one line or not.
    combo_button_orientation_ = views::LayoutOrientation::kHorizontal;

    const int baseline_height = GetBaselineMinHeight(show_segmented_control);
    host_size.SetToMax(gfx::Size(0, baseline_height));

    // If there is not enough space for the buttons on a single line with
    // caption buttons, shift them below.
    const bool wrapped_due_to_overflow = !keep_horizontal_layout &&
                                         size_bounds.width().is_bounded() &&
                                         WillWrapDueToOverflow(available_width);

    // In collapsed mode with expand on hover, the vertical tab strip is
    // positioned below the caption buttons exclusion zone, so the collapse
    // button does not need to be shifted over.
    const int left_alignment =
        (wrapped_due_to_overflow || is_exiting_expand_on_hover_ ||
         (is_collapsed && is_expand_on_hover) ||
         (keep_horizontal_layout &&
          available_width < tabs::kVerticalTabStripCollapseSnapWidth))
            ? 0
            : caption_button_width_;

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

    int current_y = 0;
    if (collapse_button_) {
      const gfx::Size pref_size = collapse_button_->GetPreferredSize();
      gfx::Rect bounds(left_alignment,
                       std::max(0, y_baseline - pref_size.height() / 2),
                       pref_size.width(), pref_size.height());
      layout.child_layouts.emplace_back(collapse_button_.get(),
                                        /*visible=*/true, bounds);
      current_y =
          bounds.bottom() +
          GetLayoutConstant(
              LayoutConstant::kVerticalTabStripCollapsedVerticalPadding);
    }

    const int collapsed_width =
        tabs::kVerticalTabStripCollapsedWidth -
        2 * GetLayoutConstant(
                LayoutConstant::kVerticalTabStripHorizontalPadding);
    const int expected_uncollapsed_width =
        uncollapsed_width -
        2 * GetLayoutConstant(
                LayoutConstant::kVerticalTabStripHorizontalPadding);
    const int layout_width =
        keep_horizontal_layout
            ? std::max(host_size.width(), expected_uncollapsed_width)
            : host_size.width();
    int right_alignment = layout_width;

    bool wrap_during_animation =
        !keep_horizontal_layout && caption_button_width_ != 0 &&
        available_width < GetPreferredWidth(show_segmented_control);

    const bool hide_trailing_control =
        keep_horizontal_layout &&
        (is_collapsing || (is_collapsed && available_width <= collapsed_width));

    if (segmented_control_) {
      const bool visible = !hide_trailing_control && show_segmented_control;
      gfx::Rect bounds;
      if (visible) {
        const gfx::Size pref_size = segmented_control_->GetPreferredSize();
        right_alignment -= pref_size.width();
        bounds =
            gfx::Rect(wrap_during_animation ? 0 : right_alignment,
                      wrap_during_animation
                          ? current_y
                          : std::max(0, y_baseline - pref_size.height() / 2),
                      pref_size.width(), pref_size.height());
      }
      layout.child_layouts.emplace_back(segmented_control_.get(), visible,
                                        bounds);
    }
    if (combo_button_) {
      const bool visible = !hide_trailing_control && !show_segmented_control;
      gfx::Rect bounds;
      if (visible) {
        const gfx::Size pref_size =
            combo_button_->GetPreferredSizeForOrientation(
                combo_button_orientation_);
        right_alignment -= pref_size.width();
        bounds =
            gfx::Rect(wrap_during_animation ? 0 : right_alignment,
                      wrap_during_animation
                          ? current_y
                          : std::max(0, y_baseline - pref_size.height() / 2),
                      pref_size.width(), pref_size.height());
      }
      layout.child_layouts.emplace_back(combo_button_.get(), visible, bounds);
    }
  }

  layout.host_size = host_size;

  return layout;
}

views::LabelButton* VerticalTabStripTopContainer::AddChildButtonFor(
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

TabStripComboButton* VerticalTabStripTopContainer::GetComboButton() {
  return combo_button_.get();
}

VerticalTabStripSegmentedControl*
VerticalTabStripTopContainer::GetSegmentedControl() {
  return segmented_control_.get();
}

bool VerticalTabStripTopContainer::IsPositionInWindowCaption(
    const gfx::Point& point) {
  if (segmented_control_ && segmented_control_->GetVisible() &&
      IsHitInView(segmented_control_, point)) {
    return false;
  }

  if (combo_button_ && combo_button_->GetVisible() &&
      IsHitInView(combo_button_, point)) {
    return false;
  }

  if (collapse_button_ && collapse_button_->GetVisible() &&
      IsHitInView(collapse_button_, point)) {
    return false;
  }

  return true;
}

void VerticalTabStripTopContainer::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  if (source == collapse_button_) {
    BrowserUserEducationInterface::From(browser_)
        ->NotifyFeaturePromoFeatureUsed(
            feature_engagement::kIPHVerticalTabsExpandOnHoverFeature,
            FeaturePromoFeatureUsedAction::kClosePromoIfPresent);

    // Reset the menu runner to avoid issues when the collapse button is
    // clicked multiple times.
    context_menu_runner_.reset();

    context_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
    context_menu_model_->AddCheckItem(
        IDC_TOGGLE_VERTICAL_TABS_EXPAND_ON_HOVER,
        l10n_util::GetStringUTF16(
            IDS_VERTICAL_TABS_COLLAPSE_BUTTON_TOGGLE_EXPAND_ON_HOVER));
    context_menu_model_->SetElementIdentifierAt(
        context_menu_model_->GetItemCount() - 1,
        kToggleVerticalTabsExpandOnHoverElementId);

    int32_t menu_runner_flags =
        views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU;

    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
    CHECK(browser_view);
    CHECK(browser_view->tab_strip_view());
    expand_on_hover_lock_ =
        browser_view->tab_strip_view()->GetExpandOnHoverLock(
            ExpandOnHoverLockType::kKeepCurrentState);

    // `base::Unretained(this)` is safe because `context_menu_runner_` is owned
    // by `this`, ensuring the callback cannot outlive `this`.
    auto on_menu_closed = base::BindRepeating(
        &VerticalTabStripTopContainer::OnCollapseButtonContextMenuClosed,
        base::Unretained(this));

    context_menu_runner_ = std::make_unique<views::MenuRunner>(
        context_menu_model_.get(), menu_runner_flags,
        std::move(on_menu_closed));

    context_menu_runner_->RunMenuAt(
        source->GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
        views::MenuAnchorPosition::kTopLeft, source_type);
  }
}

bool VerticalTabStripTopContainer::IsCommandIdChecked(int command_id) const {
  if (command_id == IDC_TOGGLE_VERTICAL_TABS_EXPAND_ON_HOVER) {
    return state_controller_->IsExpandOnHoverEnabled();
  }
  return false;
}

bool VerticalTabStripTopContainer::IsCommandIdEnabled(int command_id) const {
  if (command_id == IDC_TOGGLE_VERTICAL_TABS_EXPAND_ON_HOVER) {
    return true;
  }
  return false;
}

void VerticalTabStripTopContainer::ExecuteCommand(int command_id,
                                                  int event_flags) {
  if (command_id == IDC_TOGGLE_VERTICAL_TABS_EXPAND_ON_HOVER) {
    chrome::ExecuteCommand(browser_, command_id);
  }
}

void VerticalTabStripTopContainer::SetToolbarHeightForLayout(
    int toolbar_height) {
  if (toolbar_height_ == toolbar_height) {
    return;
  }
  toolbar_height_ = toolbar_height;
  InvalidateLayout(/*avoid_propagate_during_layout=*/true);
}

void VerticalTabStripTopContainer::SetCaptionButtonWidthForLayout(
    int caption_button_width) {
  if (caption_button_width_ == caption_button_width) {
    return;
  }
  caption_button_width_ = caption_button_width;
  InvalidateLayout(/*avoid_propagate_during_layout=*/true);
}

void VerticalTabStripTopContainer::SetIsExitingExpandOnHoverForLayout(
    bool is_exiting_expand_on_hover) {
  if (is_exiting_expand_on_hover_ == is_exiting_expand_on_hover) {
    return;
  }
  is_exiting_expand_on_hover_ = is_exiting_expand_on_hover;
  InvalidateLayout(/*avoid_propagate_during_layout=*/true);
}

bool VerticalTabStripTopContainer::WillWrapDueToOverflow(
    int available_width) const {
  return caption_button_width_ > 0 &&
         GetPreferredWidth(ShouldShowSegmentedControl(available_width)) +
                 caption_button_width_ >=
             available_width;
}

void VerticalTabStripTopContainer::OnCollapseButtonContextMenuClosed() {
  expand_on_hover_lock_.reset();
}

bool VerticalTabStripTopContainer::ShouldShowSegmentedControl(
    int available_width) const {
  if (!segmented_control_ || !state_controller_ ||
      !state_controller_->ShouldDisplayVerticalTabs()) {
    return false;
  }
  if (!browser_ || !browser_->GetProfile() ||
      !browser_->GetProfile()->GetPrefs()->GetBoolean(
          prefs::kTabSearchPinnedToTabstrip)) {
    return false;
  }

  const bool is_collapsed = state_controller_->GetCollapseState() !=
                            tabs::VerticalTabStripCollapseState::kExpanded;
  if (is_collapsed && !state_controller_->IsExpandOnHoverEnabled()) {
    return false;
  }

  const int horizontal_padding =
      GetLayoutConstant(LayoutConstant::kVerticalTabStripHorizontalPadding);
  const int effective_tab_strip_width =
      is_collapsed ? std::max(available_width + 2 * horizontal_padding,
                              state_controller_->GetUncollapsedWidth())
                   : available_width + 2 * horizontal_padding;
  return effective_tab_strip_width >
         organizer_panel::kOrganizerPanelMinWidth -
             organizer_panel::kOrganizerPanelMinOverlap;
}

gfx::Size VerticalTabStripTopContainer::GetTrailingControlPreferredSize(
    bool show_segmented_control) const {
  if (show_segmented_control && segmented_control_) {
    return segmented_control_->GetPreferredSize();
  }
  if (combo_button_) {
    return combo_button_->GetPreferredSizeForOrientation(
        views::LayoutOrientation::kHorizontal);
  }
  return gfx::Size();
}

int VerticalTabStripTopContainer::GetPreferredWidth(
    bool show_segmented_control) const {
  int total_width =
      GetTrailingControlPreferredSize(show_segmented_control).width();
  if (collapse_button_) {
    total_width +=
        collapse_button_->GetPreferredSize().width() +
        GetLayoutConstant(LayoutConstant::kVerticalTabStripTopButtonPadding);
  }
  return total_width;
}

int VerticalTabStripTopContainer::GetBaselineMinHeight(
    bool show_segmented_control) const {
  int min_height =
      GetTrailingControlPreferredSize(show_segmented_control).height();
  if (collapse_button_) {
    min_height =
        std::max(min_height, collapse_button_->GetPreferredSize().height());
  }

  if (toolbar_height_ == 0) {
    min_height += GetLayoutInsets(TOOLBAR_BUTTON).height();
  }

  return std::max(toolbar_height_, min_height);
}

BEGIN_METADATA(VerticalTabStripTopContainer)
END_METADATA
