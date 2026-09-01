// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_view.h"

#include <optional>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/time/time.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/glic/browser_ui/tab_underline_controller.h"
#include "chrome/browser/glic/browser_ui/tab_underline_view.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_data.h"
#include "chrome/browser/ui/tabs/tab_muted_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/event_utils.h"
#include "chrome/browser/ui/views/frame/base_tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/themed_background.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/common/split_tab_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_drag_handler.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_collection_controller.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_utils.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_view_horizontal_layout.h"
#include "chrome/browser/ui/views/tabs/common/tab_view_vertical_layout.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "chrome/browser/ui/views/tabs/tab/alert_indicator_button.h"
#include "chrome/browser/ui/views/tabs/tab/glow_hover_controller.h"
#include "chrome/browser/ui/views/tabs/tab/tab_accessibility.h"
#include "chrome/browser/ui/views/tabs/tab/tab_close_button.h"
#include "chrome/browser/ui/views/tabs/tab/tab_icon.h"
#include "chrome/browser/ui/views/tabs/tab/tab_title.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "chrome/browser/ui/views/tabs/vertical_tab_style_views.h"
#include "chrome/browser/ui/window_metadata/window_metadata_controller.h"
#include "chrome/common/buildflags.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/contextual_tasks/public/features.h"
#include "components/tabs/public/tab_alert.h"
#include "components/tabs/public/tab_collection_types.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/clip_recorder.h"
#include "ui/compositor/layer.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {
constexpr int kFocusRingInset = 0.0f;

class TabHighlightPathGenerator : public views::HighlightPathGenerator {
 public:
  explicit TabHighlightPathGenerator(TabView* tab_view) : tab_view_(tab_view) {}

  // views::HighlightPathGenerator:
  SkPath GetHighlightPath(const views::View* view) override {
    return tab_view_->GetPath();
  }

 private:
  raw_ptr<TabView> tab_view_;
};

bool IsSelectionModifierDown(const ui::MouseEvent& event) {
#if BUILDFLAG(IS_MAC)
  return event.IsCommandDown();
#else
  return event.IsControlDown();
#endif
}

TabStripUserGestureDetails GetGestureDetail(const ui::Event& event) {
  TabStripUserGestureDetails gesture_detail(
      TabStripUserGestureDetails::GestureType::kOther, event.time_stamp());
  TabStripUserGestureDetails::GestureType type =
      TabStripUserGestureDetails::GestureType::kOther;
  if (event.type() == ui::EventType::kMousePressed) {
    type = TabStripUserGestureDetails::GestureType::kMouse;
  } else if (event.type() == ui::EventType::kGestureTapDown) {
    type = TabStripUserGestureDetails::GestureType::kTouch;
  }
  gesture_detail.type = type;
  return gesture_detail;
}

std::unique_ptr<TabView::LayoutManager> CreateTabViewLayout(
    TabStripOrientation orientation) {
  if (orientation == TabStripOrientation::kVertical) {
    return std::make_unique<TabViewVerticalLayout>();
  }
  return std::make_unique<TabViewHorizontalLayout>();
}
}  // namespace

class TabStyleViewDelegateImpl : public TabStyleViewDelegate {
 public:
  explicit TabStyleViewDelegateImpl(const TabView* tab_view)
      : tab_view_(tab_view) {
    CHECK(tab_view_);
  }
  ~TabStyleViewDelegateImpl() override = default;

  const views::View* GetView() const override { return tab_view_; }

  bool IsActive() const override { return tab_view_->IsActive(); }

  bool IsSelected() const override {
    return tab_view_->GetSelectionState() ==
           TabStyle::TabSelectionState::kSelected;
  }

  bool IsHovering() const override { return tab_view_->IsMouseHovered(); }

  bool IsClosing() const override { return tab_view_->IsClosing(); }
  bool IsDragging() const override { return tab_view_->IsDragging(); }

  std::optional<tab_groups::TabGroupId> GetGroup() const override {
    const tabs::TabInterface* tab_interface = tab_view_->GetTabInterface();
    return tab_interface ? tab_interface->GetGroup() : std::nullopt;
  }

  std::optional<SkColor> GetGroupColor() const override {
    const auto* controller = tab_view_->collection_node()
                                 ? tab_view_->collection_node()->GetController()
                                 : nullptr;
    return controller ? controller->GetGroupColor(tab_view_->GetTabInterface())
                      : std::nullopt;
  }

  bool IsInFocusedGroup() const override {
    const std::optional<tab_groups::TabGroupId> group = GetGroup();
    if (!group.has_value()) {
      return false;
    }
    const auto* controller = tab_view_->collection_node()
                                 ? tab_view_->collection_node()->GetController()
                                 : nullptr;
    return controller && controller->GetFocusedGroup() == group;
  }

  bool IsSplit() const override { return tab_view_->split(); }
  std::optional<split_tabs::SplitTabId> GetSplit() const override {
    const tabs::TabInterface* tab_interface = tab_view_->GetTabInterface();
    return tab_interface ? tab_interface->GetSplit() : std::nullopt;
  }

  bool IsLeftSplitTab() const override {
    if (!tab_view_->split()) {
      return false;
    }
    const views::View* parent = tab_view_->parent();
    if (!parent || !views::IsViewClass<SplitTabView>(parent)) {
      return false;
    }
    const auto& children = parent->children();
    if (children.size() < 2) {
      return true;
    }
    return tab_view_ == children[base::i18n::IsRTL() ? children.size() - 1 : 0];
  }

  bool IsRightSplitTab() const override {
    if (!tab_view_->split()) {
      return false;
    }
    const views::View* parent = tab_view_->parent();
    if (!parent || !views::IsViewClass<SplitTabView>(parent)) {
      return false;
    }
    const auto& children = parent->children();
    if (children.size() < 2) {
      return true;
    }
    return tab_view_ == children[base::i18n::IsRTL() ? 0 : children.size() - 1];
  }

  const TabStyleViewDelegate* GetAdjacentTab(bool leading) const override {
    const auto* controller = tab_view_->collection_node()
                                 ? tab_view_->collection_node()->GetController()
                                 : nullptr;
    const TabCollectionNode* adjacent_node =
        controller
            ? controller->GetAdjacentTab(tab_view_->GetTabInterface(), leading)
            : nullptr;
    const TabView* adjacent_view =
        adjacent_node ? views::AsViewClass<TabView>(adjacent_node->view())
                      : nullptr;
    return adjacent_view ? adjacent_view->tab_styling()->delegate() : nullptr;
  }

  float GetHoverAnimationValue() const override {
    return tab_view_->GetHoverAnimationValue();
  }

  float GetHoverOpacity() const override {
    return tab_view_->GetHoverOpacity();
  }

  bool IsHoverAnimationActive() const override {
    return tab_view_->IsHoverAnimationActive();
  }

  GlowHoverController* GetHoverControllerForTesting() override {
    return const_cast<TabView*>(tab_view_.get())
        ->GetHoverControllerForTesting();  // IN-TEST
  }

  BrowserFrameView* GetBrowserFrameView() const override {
    const auto* controller = tab_view_->collection_node()
                                 ? tab_view_->collection_node()->GetController()
                                 : nullptr;
    return controller ? controller->GetBrowserFrameView() : nullptr;
  }

  BrowserWindowInterface* GetBrowserWindowInterface() const override {
    tabs::TabInterface* tab_interface =
        const_cast<tabs::TabInterface*>(tab_view_->GetTabInterface());
    return tab_interface ? tab_interface->GetBrowserWindowInterface() : nullptr;
  }

  int GetTabCount() const override {
    const auto* controller = tab_view_->collection_node()
                                 ? tab_view_->collection_node()->GetController()
                                 : nullptr;
    return controller ? controller->GetTabCount() : 0;
  }

  bool IsGlassFrame() const override {
    const auto* controller = tab_view_->collection_node()
                                 ? tab_view_->collection_node()->GetController()
                                 : nullptr;
    return controller ? controller->IsGlassFrame() : false;
  }

  bool IsPinned() const override { return tab_view_->pinned_; }

  bool ShouldPaintTabBackgroundColor() const override {
    return tab_view_->should_fill_background_tab_color_;
  }

  int GetStrokeThickness() const override {
    const auto* controller = tab_view_->collection_node()
                                 ? tab_view_->collection_node()->GetController()
                                 : nullptr;
    return controller ? controller->GetStrokeThickness() : 0;
  }

 private:
  const raw_ptr<const TabView> tab_view_;
};

TabView::TabView(TabCollectionNode* collection_node)
    : HoverCardAnchorTarget(this),
      collection_node_(collection_node),
      orientation_(collection_node->orientation()),
      tab_styling_(TabStyleViews::Create(
          std::make_unique<TabStyleViewDelegateImpl>(this),
          orientation_)),
      icon_(AddChildView(std::make_unique<TabIcon>())),
      title_(AddChildView(std::make_unique<TabTitle>())),
      alert_indicator_(
          AddChildView(std::make_unique<AlertIndicatorButton>(this))),
      close_button_(AddChildView(std::make_unique<TabCloseButton>(
          base::BindRepeating(&TabView::CloseButtonPressed,
                              base::Unretained(this)),
          // TODO(crbug.com/467733947): Hook up metrics logging callback.
          base::DoNothingAs<void(views::View*, const ui::MouseEvent&)>()))),
      hover_controller_(gfx::Animation::ShouldRenderRichAnimation()
                            ? std::make_unique<GlowHoverController>(
                                  this,
                                  kGlowHoverAnimationDuration)
                            : nullptr) {
  tabs::TabInterface* tab = const_cast<tabs::TabInterface*>(GetTabInterface());
  BrowserWindowInterface* browser_window = tab->GetBrowserWindowInterface();

  bool should_create_underline = false;
  if (browser_window) {
    Profile* profile = browser_window->GetProfile();
    should_create_underline =
        (glic::GlicEnabling::IsProfileEligible(profile) ||
         contextual_tasks::IsContextualTasksUIEnabled() ||
         (dictation::DictationKeyedService::Get(profile)));
  }

  if (should_create_underline) {
    glic_tab_underline_view_ =
        AddChildView(views::Builder<glic::TabUnderlineView>(
                         glic::TabUnderlineView::Factory::Create(
                             std::make_unique<glic::TabUnderlineController>(
                                 tab->GetHandle()),
                             browser_window, tab->GetHandle()))
                         .Build());
    const bool is_horizontal = orientation_ == TabStripOrientation::kHorizontal;
    glic_tab_underline_view_->SetOrientation(
        is_horizontal ? glic::TabUnderlineView::Orientation::kHorizontal
                      : glic::TabUnderlineView::Orientation::kVertical);
    if (is_horizontal) {
      glic_tab_underline_view_->SetInsets(tab_styling_->GetContentsInsets());
    }
  }

  title_->SetProperty(views::kElementIdentifierKey, kVerticalTabTitleElementId);
  SetProperty(views::kElementIdentifierKey, kTabElementId);
  // Layout manager must be set after child views are created because the
  // vertical layout stores pointers to those children.
  SetLayoutManager(CreateTabViewLayout(orientation_));
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  // So we get don't get enter/exit on children and don't prematurely stop the
  // hover.
  SetNotifyEnterExitOnChild(true);
  set_context_menu_controller(this);

  // Add accessibility and focus ring
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  views::FocusRing::Install(this);
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetHaloInset(kFocusRingInset);
  focus_ring->SetOutsetFocusRingDisabled(true);

  views::HighlightPathGenerator::Install(
      this, std::make_unique<TabHighlightPathGenerator>(this));

  GetViewAccessibility().SetRole(ax::mojom::Role::kTab);
  GetViewAccessibility().SetName(
      std::string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);

  ax_name_changed_subscription_ =
      GetViewAccessibility().AddStringAttributeChangedCallback(
          ax::mojom::StringAttribute::kName,
          base::BindRepeating(&TabView::OnAXNameChanged,
                              base::Unretained(this)));
  node_destroyed_subscription_ = collection_node_->RegisterWillDestroyCallback(
      base::BindOnce(&TabView::ResetCollectionNode, base::Unretained(this)));
  tab_state_changed_subscription_ =
      collection_node_->RegisterTabSelectionChangedCallback(base::BindRepeating(
          &TabView::OnTabStateChanged, base::Unretained(this)));
  tab_data_observer_ = std::make_unique<tabs::TabDataObserver>(tab);
  tab_data_changed_subscription_ =
      tab_data_observer_->RegisterTabDataChangedCallback(base::BindRepeating(
          &TabView::OnTabDataChanged, base::Unretained(this)));

  CHECK(collection_node_->GetController());
  if (orientation_ == TabStripOrientation::kVertical) {
    auto* state_controller =
        collection_node_->GetController()->GetStateController();
    CHECK(state_controller);
    OnCollapseStateChanged(state_controller->GetCollapseState());
    collapsed_state_changed_subscription_ =
        state_controller->RegisterOnCollapseChanged(base::BindRepeating(
            &TabView::OnCollapseStateChanged, base::Unretained(this)));
  }
  close_button_observation_.Observe(close_button_);
}

TabView::~TabView() = default;

void TabView::LayoutManager::OnInstalled(views::View* host) {
  CHECK(IsViewClass<class TabView>(host));
}

const TabView& TabView::LayoutManager::TabView() const {
  return static_cast<const class TabView&>(*host_view());
}

void TabView::StepLoadingAnimation(const base::TimeDelta& elapsed_time) {
  // TODO(crbug.com/467710547): Paint favicon to a layer when tab strip isn't
  // animating or when dragging isn't in progress or in full screen mode.
  icon_->StepLoadingAnimation(elapsed_time);
}

void TabView::CreateFreezingVote(FreezingVoteReason reason) {
  auto& vote = GetFreezingVote(reason);
  if (!vote.has_value()) {
    if (const tabs::TabInterface* tab = GetTabInterface()) {
      vote.emplace(tab->GetContents());
    }
  }
}

void TabView::ReleaseFreezingVote(FreezingVoteReason reason) {
  GetFreezingVote(reason).reset();
}

bool TabView::HasFreezingVote(FreezingVoteReason reason) const {
  switch (reason) {
    case FreezingVoteReason::kCollapsedGroup:
      return collapsed_freezing_vote_.has_value();
    case FreezingVoteReason::kFocusedGroup:
      return focus_mode_freezing_vote_.has_value();
  }
  NOTREACHED();
}

bool TabView::HasFreezingVote() const {
  return collapsed_freezing_vote_.has_value() ||
         focus_mode_freezing_vote_.has_value();
}

void TabView::UpdateFocusFreezing() {
  if (!features::IsTabGroupsFocusFreezingEnabled()) {
    return;
  }
  const tabs::TabInterface* tab = GetTabInterface();
  if (!tab) {
    return;
  }
  const auto* controller =
      collection_node_ ? collection_node_->GetController() : nullptr;
  if (!controller) {
    return;
  }
  const std::optional<tab_groups::TabGroupId> focused_group =
      controller->GetFocusedGroup();
  if (focused_group.has_value() && !tab->IsPinned() &&
      tab->GetGroup() != focused_group.value()) {
    CreateFreezingVote(FreezingVoteReason::kFocusedGroup);
  } else {
    ReleaseFreezingVote(FreezingVoteReason::kFocusedGroup);
  }
}

void TabView::UpdateHovered(bool hovered) {
  if (hovered_ == hovered) {
    return;
  }

  hovered_ = hovered;
  if (hover_controller_ && !split_) {
    if (hovered_) {
      hover_controller_->SetSubtleOpacityScale(radial_highlight_opacity_);
      hover_controller_->Show(TabStyle::ShowHoverStyle::kSubtle);
    } else {
      hover_controller_->Hide(TabStyle::HideHoverStyle::kGradual);
    }
  }

  UpdateColors();
  InvalidateLayout();
}

bool TabView::IsHoverAnimationActive() const {
  if (split_) {
    auto* split_view = views::AsViewClass<SplitTabView>(parent());
    // Ask the parent if its hover animation is running.
    return split_view &&
           (hovered_ || (split_view->hover_controller() &&
                         split_view->hover_controller()->ShouldDraw()));
  }

  return hovered_ || (hover_controller_ && hover_controller_->ShouldDraw());
}

std::optional<SkColor> TabView::GetBackgroundColor() {
  if (active_ || IsHoverAnimationActive() ||
      should_fill_background_tab_color_) {
    return tab_styling()->CalculateTargetColors().background_color;
  }
  return std::nullopt;
}

SkPath TabView::GetPath() const {
  return tab_styling()->GetPath(TabStyle::PathType::kHighlight, 1.0f,
                                {.render_units = TabStyle::RenderUnits::kDips});
}

void TabView::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);
  alert_indicator_->UpdateAlertIndicatorAnimation();
  if (orientation_ == TabStripOrientation::kHorizontal) {
    icon_->ResizeDiscardIndicatorRadiusForWidth(
        width() - 2 * tab_styling()->tab_style()->GetBottomCornerRadius());
  }
}

bool TabView::OnKeyPressed(const ui::KeyEvent& event) {
  CHECK(collection_node_);

  if (event.key_code() == ui::VKEY_RETURN && !selected_) {
    collection_node_->GetController()->SelectTab(GetTabInterface(),
                                                 GetGestureDetail(event));
    return true;
  }

  std::optional<event_utils::ReorderDirection> reorder_direction =
      event_utils::GetReorderCommandForKeyboardEvent(
          event, views::LayoutOrientation::kVertical);
  if (!reorder_direction) {
    return false;
  }

  bool move_to_end = event.flags() & ui::EF_SHIFT_DOWN;
  switch (*reorder_direction) {
    case event_utils::ReorderDirection::kPrevious: {
      if (move_to_end) {
        collection_node_->GetController()->MoveTabFirst(GetTabInterface());
      } else {
        collection_node_->GetController()->ShiftTabPrevious(GetTabInterface());
      }
      break;
    }
    case event_utils::ReorderDirection::kNext: {
      if (move_to_end) {
        collection_node_->GetController()->MoveTabLast(GetTabInterface());
      } else {
        collection_node_->GetController()->ShiftTabNext(GetTabInterface());
      }
      break;
    }
  }

  return true;
}

bool TabView::OnKeyReleased(const ui::KeyEvent& event) {
  CHECK(collection_node_);

  if (event.key_code() == ui::VKEY_SPACE && !selected_) {
    collection_node_->GetController()->SelectTab(GetTabInterface(),
                                                 GetGestureDetail(event));
    return true;
  }
  return false;
}

bool TabView::OnMousePressed(const ui::MouseEvent& event) {
  CHECK(collection_node_);

  auto* controller = collection_node_->GetController();
  shift_pressed_on_mouse_down_ = event.IsShiftDown();
  RecordMousePressedInTab();
  UpdateHoverCard(nullptr, TabSlotController::HoverCardUpdateType::kEvent);

  // Capture the selection model before selection changes.
  const ui::ListSelectionModel original_selection_model =
      controller->GetSelectionModel();

  if (event.IsOnlyLeftMouseButton() ||
      (event.IsOnlyRightMouseButton() && event.flags() & ui::EF_FROM_TOUCH)) {
    if (event.IsShiftDown() && IsSelectionModifierDown(event)) {
      controller->AddSelectionFromAnchorTo(GetTabInterface());
      base::RecordAction(
          base::UserMetricsAction("TabMultiSelect_AddSelectionFromAnchorTo"));
    } else if (event.IsShiftDown()) {
      controller->ExtendSelectionTo(GetTabInterface());
      base::RecordAction(
          base::UserMetricsAction("TabMultiSelect_ExtendSelectionTo"));
    } else if (IsSelectionModifierDown(event)) {
      controller->ToggleSelected(GetTabInterface());
      base::RecordAction(
          base::UserMetricsAction("TabMultiSelect_ToggleSelected"));
      if (!selected_) {
        return false;
      }
    } else if (!selected_) {
      controller->SelectTab(GetTabInterface(), GetGestureDetail(event));
      base::RecordAction(base::UserMetricsAction("SwitchTab_Click"));
    }
    // Potentially start the drag for the mouse press.
    // Follow-up mouse-movement events will update the drag controller and
    // eventually kick off the drag-loop.
    controller->GetDragHandler().InitializeDrag(
        *collection_node_, original_selection_model, event);
  }
  return true;
}

void TabView::OnMouseReleased(const ui::MouseEvent& event) {
  CHECK(collection_node_);

  auto* controller = collection_node_->GetController();
  base::WeakPtr<TabView> self = weak_ptr_factory_.GetWeakPtr();
  if (event.IsOnlyMiddleMouseButton()) {
    if (HitTestPoint(event.location())) {
      controller->CloseTab(GetTabInterface(), CloseTabSource::kFromMouse);
    }
  } else if (event.IsOnlyLeftMouseButton() &&
             !(event.IsShiftDown() || shift_pressed_on_mouse_down_) &&
             !IsSelectionModifierDown(event)) {
    controller->SelectTab(GetTabInterface(), GetGestureDetail(event));
  }
  // Cancel the initialized drag (noop if not started). This is considered
  // a cancel because the drag handler assumes mouse capture when the drag
  // loop starts.
  controller->GetDragHandler().EndDrag(EndDragReason::kCancel);
  if (!self) {
    return;
  }
  shift_pressed_on_mouse_down_ = false;
}

void TabView::OnMouseMoved(const ui::MouseEvent& event) {
  // Hover state is handled by the parent if it is split.
  if (split_) {
    return;
  }
  // Windows synthesizes mouse move events if the user does a touch drag.
  // Don't set the hover state for those events.
  if (event.flags() & ui::EF_FROM_TOUCH) {
    return;
  }
  // Linux enter/leave events are sometimes flaky, so we don't want to "miss"
  // an enter event and fail to hover the tab.
  UpdateHovered(true);
}

void TabView::OnMouseEntered(const ui::MouseEvent& event) {
  CHECK(collection_node_);
  UpdateHoverCard(this, TabSlotController::HoverCardUpdateType::kHover);

  // Hover state is handled by the parent if it is split.
  if (split_) {
    return;
  }
  // Windows synthesizes mouse events if the user does a touch drag.
  // Don't set the hover state for those events.
  if (event.flags() & ui::EF_FROM_TOUCH) {
    return;
  }

  UpdateHovered(true);
}

void TabView::OnMouseExited(const ui::MouseEvent& event) {
  CHECK(collection_node_);

  // Hover state is handled by the parent if it is split.
  if (split_) {
    return;
  }

  UpdateHovered(false);
}

bool TabView::OnMouseDragged(const ui::MouseEvent& event) {
  // Protect against key presses when the tab is animating out. Drag events may
  // call this function after the node has been deleted.
  if (!collection_node_) {
    return false;
  }

  auto* controller = collection_node_->GetController();
  CHECK(controller);
  return controller->GetDragHandler().ContinueDrag(*this, event);
}

void TabView::OnGestureEvent(ui::GestureEvent* event) {
  CHECK(collection_node_);
  UpdateHoverCard(nullptr, TabSlotController::HoverCardUpdateType::kEvent);

  auto* controller = collection_node_->GetController();
  CHECK(controller);

  const ui::ListSelectionModel original_selection_model =
      collection_node_->GetController()->GetSelectionModel();

  switch (event->type()) {
    case ui::EventType::kGestureTapDown: {
      // Handle TapDown to receive subsequent events like LongPress or Tap.
      // We don't call InitializeDrag here to allow scrolling.
      event->SetHandled();
      break;
    }

    case ui::EventType::kGestureTap: {
      // Short press release. Select the tab.
      if (!selected_) {
        controller->SelectTab(GetTabInterface(), GetGestureDetail(*event));
      }
      event->SetHandled();
      break;
    }

    case ui::EventType::kGestureLongPress: {
      // Long press detected. Initialize dragging.
      if (!selected_) {
        // Ensure the tab is selected before dragging starts to avoid crashes.
        controller->SelectTab(GetTabInterface(), GetGestureDetail(*event));
      }
      controller->GetDragHandler().InitializeDrag(
          *collection_node_, original_selection_model, *event);
      event->SetHandled();
      break;
    }

    case ui::EventType::kGestureLongTap: {
      // Show context menu on release after long press.
      controller->ShowTabContextMenu(collection_node_, event->location(),
                                     ui::mojom::MenuSourceType::kTouch);
      event->SetHandled();
      break;
    }

    default:
      break;
  }
}

void TabView::PaintChildren(const views::PaintInfo& info) {
  ui::ClipRecorder clip_recorder(info.context());
  // The paint recording scale for tabs is consistent along the x and y axis.
  const float paint_recording_scale = info.paint_recording_scale_x();

  if (const std::optional<SkPath> clip_path =
          tab_styling()->GetChildClipPath(paint_recording_scale)) {
    clip_recorder.ClipPathWithAntiAliasing(clip_path.value());
  }

  View::PaintChildren(info);
}

void TabView::OnPaint(gfx::Canvas* canvas) {
  // Split pinned tabs have a merged background that is rendered in
  // `SplitTabView`.
  if (pinned_ && split_) {
    return;
  }

  tab_styling()->PaintTab(canvas);

  views::View::OnPaint(canvas);
}

void TabView::AddedToWidget() {
  paint_as_active_subscription_ =
      GetWidget()->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &TabView::OnFrameActiveStateChanged, base::Unretained(this)));

  OnTabStateChanged();

  // Recompute accessible name when the structure changes.
  UpdateAccessibleName();

  // Recompute the hovered state as mouse events are not processed if a view
  // removed from the widget and added.
  if (!split_) {
    UpdateHovered(IsMouseHovered());
  }
}

void TabView::RemovedFromWidget() {
  paint_as_active_subscription_ = {};
  UpdateHovered(false);
}

void TabView::OnFocus() {
  views::View::OnFocus();

  if (collection_node_ && collection_node_->GetController()) {
    collection_node_->GetController()->TabKeyboardFocusChangedTo(
        GetTabInterface());
  }

  // Update the accessible label before showing the hover card to ensure that
  // the displayed memory usage is in sync with what will be read to screen
  // readers.
  UpdateAccessibleName();
  UpdateHoverCard(this, TabSlotController::HoverCardUpdateType::kFocus);
  InvalidateLayout();
}

void TabView::OnBlur() {
  views::View::OnBlur();

  if (collection_node_ && collection_node_->GetController()) {
    collection_node_->GetController()->TabKeyboardFocusChangedTo(nullptr);
  }

  if (auto* tab_strip_view = GetTabStripView(this)) {
    if (!tab_strip_view->IsFocusInTabStrip()) {
      UpdateHoverCard(nullptr, TabSlotController::HoverCardUpdateType::kFocus);
    }
  }
  InvalidateLayout();
}

gfx::Size TabView::GetMinimumSize() const {
  if (orientation_ == TabStripOrientation::kHorizontal) {
    if (pinned_) {
      return gfx::Size(tab_styling()->tab_style()->GetPinnedWidth(split_),
                       tab_styling()->tab_style()->GetStandardHeight());
    }
    const int min_width =
        active_ ? tab_styling()->tab_style()->GetMinimumActiveWidth(split_)
                : tab_styling()->tab_style()->GetMinimumInactiveWidth();
    return gfx::Size(min_width,
                     tab_styling()->tab_style()->GetStandardHeight());
  }
  return views::View::GetMinimumSize();
}

void TabView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (orientation_ == TabStripOrientation::kVertical) {
    SetClipPath(GetPath());
  }
}

void TabView::UpdateParentLayer() {
  views::View::UpdateParentLayer();
  if (layer()) {
    UpdateLayerRoundedCorners();
  }
}

void TabView::OnViewFocused(views::View* observed_view) {
  if (observed_view == close_button_) {
    InvalidateLayout();
  }
}

void TabView::OnViewBlurred(views::View* observed_view) {
  if (observed_view == close_button_) {
    InvalidateLayout();
  }
}

void TabView::UpdateLayerRoundedCorners() {
  const SkScalar corner_radius = GetCornerRadius();
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(corner_radius));
  layer()->SetIsFastRoundedCorner(true);
}

void TabView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateColors();
}

bool TabView::GetHitTestMask(SkPath* mask) const {
  *mask = GetPath();
  return true;
}

bool TabView::ShouldEnableMuteToggle(int required_width) {
  if (active_) {
    return true;
  }

  if (!alert_indicator_->GetVisible()) {
    return false;
  }

  return alert_indicator_->x() >= required_width;
}

void TabView::ToggleTabAudioMute() {
  // The TabAlertIndicator can call this function even after the node has been
  // deleted, so prevent calling if collection node doesnt exist.
  if (!collection_node_) {
    return;
  }

  content::WebContents* const contents = GetTabInterface()->GetContents();
  bool mute = !contents->IsAudioMuted();
  base::UmaHistogramBoolean("Media.Audio.TabAudioMuted", mute);
  SetTabAudioMuted(contents, mute, TabMutedReason::kAudioIndicator,
                   /*extension_id=*/std::string());
}

bool TabView::IsApparentlyActive() const {
  if (active_) {
    return true;
  }
  if (hovered_) {
    return GetHoverOpacity() > 0.5f;
  }
  return selected_;
}

void TabView::AlertStateChanged() {
  // TODO(crbug.com/457525548): Update hover card.
  InvalidateLayout();
}

void TabView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  if (collection_node_) {
    if (auto* controller = collection_node_->GetController()) {
      controller->ShowTabContextMenu(collection_node_, point, source_type);
    }
  }
}

bool TabView::NeedsToShowThumbnail() const {
  return !IsActive();
}

bool TabView::IsValidHoverCardTarget() const {
  return collection_node_ && !IsDragging();
}

views::BubbleBorder::Arrow TabView::GetAnchorPosition() const {
  if (orientation_ == TabStripOrientation::kHorizontal) {
    return views::BubbleBorder::Arrow::TOP_LEFT;
  }
  if (pinned_ && !collapsed_) {
    return views::BubbleBorder::Arrow::TOP_LEFT;
  }
  return views::BubbleBorder::Arrow::LEFT_TOP;
}

views::BubbleAnchor TabView::GetAnchor() {
  if (split_ && !collapsed_) {
    return views::BubbleAnchor(parent());
  }
  return views::BubbleAnchor(this);
}

void TabView::ResetCollectionNode() {
  CHECK(collection_node_);

  // Fetch the hover card controller before we clear `collection_node_`.
  TabHoverCardController* const hover_card_controller =
      collection_node_->GetController()
          ? collection_node_->GetController()->GetHoverCardController()
          : nullptr;

  // Clear all observers and subscriptions immediately to prevent any callback
  // execution during the rest of this cleanup.
  tab_data_changed_subscription_ = base::CallbackListSubscription();
  tab_state_changed_subscription_ = base::CallbackListSubscription();
  collapsed_state_changed_subscription_ = base::CallbackListSubscription();
  node_destroyed_subscription_ = base::CallbackListSubscription();

  collection_node_ = nullptr;

  if (hover_card_controller && hover_card_controller->target_tab() == this) {
    hover_card_controller->UpdateHoverCard(
        nullptr, TabSlotController::HoverCardUpdateType::kTabRemoved);
  }

  // Reset the active/selected/hovered states so the tab animates out without a
  // background.
  active_ = false;
  selected_ = false;

  // Update the callbacks for the buttons so that we don't call anything that
  // needs the node.
  close_button_->SetCallback(base::RepeatingClosure(base::DoNothing()));

  static_cast<TabView::LayoutManager*>(GetLayoutManager())->OnTabClosing();
}

void TabView::UpdateAccessibleName() {
  CHECK(collection_node_);

  std::u16string name =
      tabs::GetAccessibleTabLabel(GetTabInterface(), /*is_for_tab=*/true);
  if (!name.empty()) {
    GetViewAccessibility().SetName(name);
  } else {
    GetViewAccessibility().SetName(
        std::string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
  }
}

void TabView::OnFrameActiveStateChanged() {
  UpdateColors();
}

void TabView::OnAXNameChanged(ax::mojom::StringAttribute attribute,
                              const std::optional<std::string>& name) {
  if (GetWidget() && active_) {
    GetWidget()->UpdateAccessibleNameForRootView();
  }
}

void TabView::OnCollapseStateChanged(
    tabs::VerticalTabStripCollapseState state) {
  collapsed_ = state == tabs::VerticalTabStripCollapseState::kCollapsed;
}

void TabView::OnTabStateChanged() {
  CHECK(collection_node_);

  tabs::TabInterface* tab = const_cast<tabs::TabInterface*>(GetTabInterface());
  CHECK(tab);

  const TabStripModel* tab_strip_model =
      tab->GetBrowserWindowInterface()->GetTabStripModel();
  int index = tab_strip_model->GetIndexOfTab(tab);
  CHECK(index != TabStripModel::kNoTab);

  active_ = tab_strip_model->IsTabInForeground(index);
  split_ = tab->IsSplit();
  pinned_ = tab->IsPinned();

  SetSelection(tab->IsSelected());
  UpdateTabData(tab);

  UpdateFocusFreezing();

  UpdateColors();
  InvalidateLayout();
}

void TabView::OnTabDataChanged(TabChangeType change_type,
                               const tabs::TabData& data) {
  CHECK(collection_node_);

  // Update the accessible name when the hovered tab's memory usage changes
  // so screen readers are in sync with the hover card. Skips updating
  // other visual UI elements (title, favicon, alert buttons) because those
  // states usually do not change when memory is updated.
  if (change_type == TabChangeType::kResourceUsageOnly) {
    if (IsActive() || HasFocus()) {
      UpdateAccessibleName();
    }
    return;
  }
  UpdateTabData(GetTabInterface());
}

void TabView::SetSelection(bool selected) {
  if (selected_ == selected) {
    return;
  }

  selected_ = selected;
  GetViewAccessibility().SetIsSelected(selected_);
}

void TabView::UpdateTabData(const tabs::TabInterface* tab) {
  tabs::TabData old_data = std::move(tab_data_);
  tab_data_ = tab_data_observer_->tab_data();

  if (tabs::ShouldUpdateAccessibleName(old_data, tab_data_)) {
    UpdateAccessibleName();
  }

  icon_->SetData(tab_data_);
  icon_->SetActiveState(tab->IsActivated());
  icon_->SetAttention(TabIcon::AttentionType::kBlockedWebContents,
                      !tab->IsActivated() && tab->IsBlocked());
  icon_->SetAttention(TabIcon::AttentionType::kTabWantsAttentionStatus,
                      tab_data_.needs_attention);
  UpdateTitle(tab_data_.title, tab_data_.should_render_loading_title);
  alert_indicator_->TransitionToAlertState(tab_data_.alert_state);
  SetHoverCardDataFrom(tab_data_);
}

void TabView::SetDataForTesting(tabs::TabData data) {
  tabs::TabData old_data = std::move(tab_data_);
  tab_data_ = std::move(data);

  if (tabs::ShouldUpdateAccessibleName(old_data, tab_data_)) {
    UpdateAccessibleName();
  }

  icon_->SetData(tab_data_);
  icon_->SetAttention(TabIcon::AttentionType::kTabWantsAttentionStatus,
                      tab_data_.needs_attention);
  UpdateTitle(tab_data_.title, tab_data_.should_render_loading_title);
  alert_indicator_->TransitionToAlertState(tab_data_.alert_state);
  SetHoverCardDataFrom(tab_data_);
}

void TabView::UpdateTitle(std::u16string title,
                          bool should_render_loading_title) {
  if (should_render_loading_title) {
    title = icon_->GetShowingLoadingAnimation()
                ? l10n_util::GetStringUTF16(IDS_TAB_LOADING_TITLE)
                : CoreTabHelper::GetDefaultTitle();
  } else {
    title = WindowMetadataController::FormatTitleForDisplay(title);
  }
  title_->SetText(title);
}

void TabView::UpdateBorder() {
  if (orientation_ == TabStripOrientation::kHorizontal) {
    SetBorder(views::CreateEmptyBorder(tab_styling()->GetContentsInsets()));
    return;
  }

  if (pinned_) {
    if (split_) {
      // Insets for border handled by the `SplitTabView`.
      SetBorder(views::CreateEmptyBorder(gfx::Insets(GetLayoutConstant(
          LayoutConstant::kVerticalTabPinnedBorderThickness))));
    } else {
      SetBorder(views::CreateRoundedRectBorder(
          GetLayoutConstant(LayoutConstant::kVerticalTabPinnedBorderThickness),
          GetLayoutConstant(LayoutConstant::kVerticalTabCornerRadius),
          IsFrameActive() ? kColorVerticalTabPinnedOutline
                          : kColorTabDividerFrameInactive));
    }
  } else if (GetBorder()) {
    SetBorder(nullptr);
  }
}

void TabView::UpdateColors() {
  UpdateContrastRatioValues();
  if (auto* theme_provider = GetThemeProvider()) {
    should_fill_background_tab_color_ = theme_provider->GetDisplayProperty(
        ThemeProperties::SHOULD_FILL_BACKGROUND_TAB_COLOR);
  }
  TabStyle::TabColors colors = tab_styling()->CalculateTargetColors();
  title_->SetEnabledColor(colors.foreground_color);
  close_button_->SetColors(colors);
  alert_indicator_->OnParentTabButtonColorChanged();

  UpdateBorder();

  // TODO(crbug.com/465159185): Update focus ring colors.
  SchedulePaint();
}

void TabView::UpdateContrastRatioValues() {
  auto [hover_opacity_min, hover_opacity_max, radial_highlight_opacity, _] =
      tab_styling()->tab_style()->GetContrastRatioValues(IsFrameActive(),
                                                         GetColorProvider());
  hover_opacity_min_ = hover_opacity_min;
  hover_opacity_max_ = hover_opacity_max;
  radial_highlight_opacity_ = radial_highlight_opacity;
}

void TabView::CloseButtonPressed(const ui::Event& event) {
  CHECK(collection_node_);

  if (active_) {
    base::RecordAction(base::UserMetricsAction("CloseTab_Active"));
  } else {
    base::RecordAction(base::UserMetricsAction("CloseTab_Inactive"));
  }

  CHECK(alert_indicator_);
  if (!alert_indicator_->GetVisible()) {
    base::RecordAction(base::UserMetricsAction("CloseTab_NoAlertIndicator"));
  } else if (tab_data_.alert_state.has_value()) {
    tabs::TabAlertController::RecordCloseTabMetrics(
        tab_data_.alert_state.value());
  }

  if (split_) {
    auto* split_view = views::AsViewClass<SplitTabView>(parent());
    base::RecordAction(base::UserMetricsAction(this == split_view->children()[0]
                                                   ? "CloseTab_StartTabInSplit"
                                                   : "CloseTab_EndTabInSplit"));
  }

  // Hide the interactive close button while the tab is animating out.
  if (close_button_) {
    close_button_->SetVisible(false);
  }

  const bool from_mouse = event.type() == ui::EventType::kMouseReleased &&
                          !(event.flags() & ui::EF_FROM_TOUCH);
  collection_node_->GetController()->CloseTab(
      GetTabInterface(),
      from_mouse ? CloseTabSource::kFromMouse : CloseTabSource::kFromTouch);
}

void TabView::RecordMousePressedInTab() {
  auto* tab_strip_view = GetTabStripView(this);
  CHECK(tab_strip_view);
  tab_strip_view->RecordMousePressedInTab();
}

double TabView::GetHoverAnimationValue() const {
  if (split_) {
    if (auto* split_view = views::AsViewClass<SplitTabView>(parent())) {
      return split_view->GetHoverAnimationValue();
    }
  }
  return hover_controller_ ? hover_controller_->GetAnimationValue()
                           : (hovered_ ? 1.0 : 0.0);
}

float TabView::GetHoverOpacity() const {
  // Opacity boost varies on tab width.  The interpolation is nonlinear so
  // that most tabs will fall on the low end of the opacity range, but very
  // narrow tabs will still stand out on the high end.
  // TODO(crbug.com/457525745): Determine what the min and max widths should be.
  constexpr float kWidthForMinHoverOpacity = 216.0f;
  constexpr float kWidthForMaxHoverOpacity = 32.0f;
  const float value_in_range = static_cast<float>(width());
  const float t =
      std::clamp((kWidthForMinHoverOpacity - value_in_range) /
                     (kWidthForMinHoverOpacity - kWidthForMaxHoverOpacity),
                 0.0f, 1.0f);
  return gfx::Tween::FloatValueBetween(t * t, hover_opacity_min_,
                                       hover_opacity_max_);
}

bool TabView::IsFrameActive() const {
  return GetWidget() ? GetWidget()->ShouldPaintAsActive() : true;
}

TabStyle::TabSelectionState TabView::GetSelectionState() const {
  return active_ ? TabStyle::TabSelectionState::kActive
                 : (selected_ ? TabStyle::TabSelectionState::kSelected
                              : TabStyle::TabSelectionState::kInactive);
}

bool TabView::IsDragging() const {
  if (!collection_node_ || !collection_node_->GetController()) {
    return false;
  }
  const auto& drag_handler =
      collection_node_->GetController()->GetDragHandler();
  if (drag_handler.IsViewDragging(*this)) {
    return true;
  }
  for (const views::View* v = parent(); v; v = v->parent()) {
    if (drag_handler.IsViewDragging(*v)) {
      return true;
    }
  }
  return false;
}

// static
int TabView::UncollapsedMinWidth() {
  // This is the width of a tab in a split that is in a tab group, while the
  // tab strip is in the narrowest uncollapsed state.
  return (VerticalTabStripRegionView::kUncollapsedMinWidth -
          2 * GetLayoutConstant(
                  LayoutConstant::kVerticalTabStripHorizontalPadding) -
          SplitTabView::kSplitViewGap - TabGroupView::kTabLeadingPadding) /
         2;
}

// static
int TabView::CollapsedWidth() {
  return VerticalTabStripRegionView::kCollapsedWidth -
         2 * GetLayoutConstant(
                 LayoutConstant::kVerticalTabStripHorizontalPadding);
}

bool TabView::IsInExpandOnHover(int width) const {
  return collapsed_ && width > CollapsedWidth();
}

const tabs::TabInterface* TabView::GetTabInterface() const {
  if (!collection_node_) {
    return nullptr;
  }
  return std::get<tabs::ConstDanglingUntriagedTabInterface>(
      collection_node_->GetNodeData());
}

void TabView::UpdateHoverCard(HoverCardAnchorTarget* target,
                              int hover_card_update_type) {
  CHECK(collection_node_);

  if (TabHoverCardController* hover_card_controller =
          collection_node_->GetController()->GetHoverCardController()) {
    hover_card_controller->UpdateHoverCard(
        target, static_cast<TabSlotController::HoverCardUpdateType>(
                    hover_card_update_type));
  }
}

SkScalar TabView::GetCornerRadius() const {
  return SkIntToScalar(
      GetLayoutConstant(LayoutConstant::kVerticalTabCornerRadius) +
      (split_ ? GetInsets().height() : 0));
}

std::optional<performance_manager::freezing::FreezingVote>&
TabView::GetFreezingVote(FreezingVoteReason reason) {
  switch (reason) {
    case FreezingVoteReason::kCollapsedGroup:
      return collapsed_freezing_vote_;
    case FreezingVoteReason::kFocusedGroup:
      return focus_mode_freezing_vote_;
  }
  NOTREACHED();
}

BEGIN_METADATA(TabView)
END_METADATA
