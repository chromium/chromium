// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"

#include <optional>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/tab_muted_utils.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/themed_background.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/tab/alert_indicator_button.h"
#include "chrome/browser/ui/views/tabs/tab/glow_hover_controller.h"
#include "chrome/browser/ui/views/tabs/tab/tab_accessibility.h"
#include "chrome/browser/ui/views/tabs/tab/tab_close_button.h"
#include "chrome/browser/ui/views/tabs/tab/tab_icon.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_split_tab_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_drag_handler.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_view.h"
#include "chrome/common/buildflags.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/browser_ui/tab_underline_view.h"
#include "chrome/browser/glic/browser_ui/tab_underline_view_controller_impl.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "components/contextual_tasks/public/features.h"
#endif

namespace {
constexpr int kIconDesignWidth = 16;
constexpr int kTitleMinWidth = 10;
constexpr int kHorizontalInset = 7;
constexpr int kDefaultPadding = 4;
constexpr int kFocusRingInset = 0.0f;

class VerticalTabHighlightPathGenerator : public views::HighlightPathGenerator {
 public:
  explicit VerticalTabHighlightPathGenerator(VerticalTabView* tab_view)
      : tab_view_(tab_view) {}

  // views::HighlightPathGenerator:
  SkPath GetHighlightPath(const views::View* view) override {
    return tab_view_->GetPath();
  }

 private:
  raw_ptr<VerticalTabView> tab_view_;
};

class VerticalTabTitle : public views::Label {
  METADATA_HEADER(VerticalTabTitle, views::Label)
 public:
  VerticalTabTitle() {
    SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
    SetElideBehavior(gfx::FADE_TAIL);
    SetHandlesTooltips(false);
    SetBackgroundColor(SK_ColorTRANSPARENT);
    SetProperty(views::kElementIdentifierKey, kVerticalTabTitleElementId);
  }
};

BEGIN_METADATA(VerticalTabTitle)
END_METADATA

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
}  // namespace

VerticalTabView::VerticalTabView(TabCollectionNode* collection_node)
    : HoverCardAnchorTarget(this),
      collection_node_(collection_node),
      tab_style_(TabStyle::Get()),
      icon_(AddChildView(std::make_unique<TabIcon>())),
      title_(AddChildView(std::make_unique<VerticalTabTitle>())),
      alert_indicator_(
          AddChildView(std::make_unique<AlertIndicatorButton>(this))),
      close_button_(AddChildView(std::make_unique<TabCloseButton>(
          base::BindRepeating(&VerticalTabView::CloseButtonPressed,
                              base::Unretained(this)),
          // TODO(crbug.com/467733947): Hook up metrics logging callback.
          base::DoNothingAs<void(views::View*, const ui::MouseEvent&)>()))),
      hover_controller_(gfx::Animation::ShouldRenderRichAnimation()
                            ? std::make_unique<GlowHoverController>(this)
                            : nullptr) {
#if BUILDFLAG(ENABLE_GLIC)
  tabs::TabInterface* tab = const_cast<tabs::TabInterface*>(GetTabInterface());
  BrowserWindowInterface* browser_window = tab->GetBrowserWindowInterface();
  if (browser_window &&
      ((base::FeatureList::IsEnabled(features::kGlicMultitabUnderlines) &&
        glic::GlicEnabling::IsProfileEligible(browser_window->GetProfile())) ||
       base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks))) {
    glic_tab_underline_view_ = AddChildView(
        views::Builder<glic::TabUnderlineView>(
            glic::TabUnderlineView::Factory::Create(
                std::make_unique<glic::TabUnderlineViewControllerImpl>(),
                browser_window->GetBrowserForMigrationOnly(), tab->GetHandle()))
            .Build());
    glic_tab_underline_view_->SetOrientation(
        glic::TabUnderlineView::Orientation::kVertical);
    glic_tab_underline_view_->SetProperty(views::kViewIgnoredByLayoutKey, true);
    glic_tab_underline_view_->SetBoundsRect(
        gfx::Rect(0, 0, 2 * glic::TabUnderlineView::kEffectThickness,
                  GetLayoutConstant(LayoutConstant::kVerticalTabHeight)));
  }
#endif

  // Ordered vector of children to be rendered in the tab.
  tab_children_configs_ = {
      TabChildConfig(close_button_, kIconDesignWidth, kDefaultPadding,
                     /*align_leading=*/false,
                     /*expand=*/false),
      TabChildConfig(alert_indicator_, kIconDesignWidth, kDefaultPadding,
                     /*align_leading=*/false,
                     /*expand=*/false),
      TabChildConfig(icon_, kIconDesignWidth, kHorizontalInset,
                     /*align_leading=*/true,
                     /*expand=*/false),
      TabChildConfig(title_, kTitleMinWidth, kDefaultPadding,
                     /*align_leading=*/true,
                     /*expand=*/true)};

  SetProperty(views::kElementIdentifierKey, kTabElementId);
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  // So we get don't get enter/exit on children and don't prematurely stop the
  // hover.
  SetNotifyEnterExitOnChild(true);

  // Add accessibility and focus ring
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  views::FocusRing::Install(this);
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetHaloInset(kFocusRingInset);
  focus_ring->SetOutsetFocusRingDisabled(true);

  views::HighlightPathGenerator::Install(
      this, std::make_unique<VerticalTabHighlightPathGenerator>(this));

  GetViewAccessibility().SetRole(ax::mojom::Role::kTab);
  GetViewAccessibility().SetName(
      std::string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);

  ax_name_changed_subscription_ =
      GetViewAccessibility().AddStringAttributeChangedCallback(
          ax::mojom::StringAttribute::kName,
          base::BindRepeating(&VerticalTabView::OnAXNameChanged,
                              base::Unretained(this)));
  node_destroyed_subscription_ =
      collection_node_->RegisterWillDestroyCallback(base::BindOnce(
          &VerticalTabView::ResetCollectionNode, base::Unretained(this)));
  data_changed_subscription_ =
      collection_node_->RegisterDataChangedCallback(base::BindRepeating(
          &VerticalTabView::OnDataChanged, base::Unretained(this)));

  CHECK(collection_node_->GetController());
  auto* state_controller =
      collection_node_->GetController()->GetStateController();
  CHECK(state_controller);
  collapsed_state_changed_subscription_ =
      state_controller->RegisterOnCollapseChanged(base::BindRepeating(
          &VerticalTabView::OnCollapsedStateChanged, base::Unretained(this)));
  collapsed_ = state_controller->IsCollapsed();

  set_context_menu_controller(this);
}

VerticalTabView::~VerticalTabView() = default;

void VerticalTabView::StepLoadingAnimation(
    const base::TimeDelta& elapsed_time) {
  // TODO(crbug.com/467710547): Paint favicon to a layer when tab strip isn't
  // animating or when dragging isn't in progress or in full screen mode.
  icon_->StepLoadingAnimation(elapsed_time);
}

void VerticalTabView::CreateFreezingVote() {
  if (!freezing_vote_.has_value()) {
    freezing_vote_.emplace(GetTabInterface()->GetContents());
  }
}

void VerticalTabView::ReleaseFreezingVote() {
  freezing_vote_.reset();
}

void VerticalTabView::UpdateHovered(bool hovered) {
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
  UpdateThemeColors();
  InvalidateLayout();
}

bool VerticalTabView::IsHoverAnimationActive() const {
  if (split_) {
    auto* split_view = views::AsViewClass<VerticalSplitTabView>(parent());
    // Ask the parent if its hover animation is running.
    return split_view &&
           (hovered_ || (split_view->hover_controller() &&
                         split_view->hover_controller()->ShouldDraw()));
  }

  return hovered_ || (hover_controller_ && hover_controller_->ShouldDraw());
}

std::optional<SkColor> VerticalTabView::GetBackgroundColor() {
  if (active_ || IsHoverAnimationActive() ||
      GetThemeProvider()->GetDisplayProperty(
          ThemeProperties::SHOULD_FILL_BACKGROUND_TAB_COLOR)) {
    return tab_style_->GetCurrentTabBackgroundColor(
        GetSelectionState(), IsHoverAnimationActive(), GetHoverAnimationValue(),
        IsFrameActive(), GetColorProvider());
  }
  return std::nullopt;
}

SkPath VerticalTabView::GetPath() const {
  const SkScalar corner_radius = SkIntToScalar(
      GetLayoutConstant(LayoutConstant::kVerticalTabCornerRadius) +
      (split_ ? GetInsets().height() : 0));
  return SkPath::RRect(SkRRect::MakeRectXY(gfx::RectToSkRect(GetLocalBounds()),
                                           corner_radius, corner_radius));
}

void VerticalTabView::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);
  alert_indicator_->UpdateAlertIndicatorAnimation();
}

bool VerticalTabView::OnKeyPressed(const ui::KeyEvent& event) {
  CHECK(collection_node_);

  if (event.key_code() == ui::VKEY_RETURN && !selected_) {
    collection_node_->GetController()->SelectTab(GetTabInterface(),
                                                 GetGestureDetail(event));
    return true;
  }
  return false;
}

bool VerticalTabView::OnKeyReleased(const ui::KeyEvent& event) {
  CHECK(collection_node_);

  if (event.key_code() == ui::VKEY_SPACE && !selected_) {
    collection_node_->GetController()->SelectTab(GetTabInterface(),
                                                 GetGestureDetail(event));
    return true;
  }
  return false;
}

bool VerticalTabView::OnMousePressed(const ui::MouseEvent& event) {
  CHECK(collection_node_);

  auto* controller = collection_node_->GetController();
  shift_pressed_on_mouse_down_ = event.IsShiftDown();
  RecordMousePressedInTab();
  UpdateHoverCard(nullptr, TabSlotController::HoverCardUpdateType::kEvent);

  if (event.IsOnlyLeftMouseButton() ||
      (event.IsOnlyRightMouseButton() && event.flags() & ui::EF_FROM_TOUCH)) {
    if (event.IsShiftDown() && IsSelectionModifierDown(event)) {
      controller->AddSelectionFromAnchorTo(GetTabInterface());
    } else if (event.IsShiftDown()) {
      controller->ExtendSelectionTo(GetTabInterface());
    } else if (IsSelectionModifierDown(event)) {
      controller->ToggleSelected(GetTabInterface());
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
    controller->GetDragHandler().InitializeDrag(*collection_node_, event);
  }
  return true;
}

void VerticalTabView::OnMouseReleased(const ui::MouseEvent& event) {
  CHECK(collection_node_);

  auto* controller = collection_node_->GetController();
  base::WeakPtr<VerticalTabView> self = weak_ptr_factory_.GetWeakPtr();
  if (event.IsOnlyMiddleMouseButton()) {
    controller->CloseTab(GetTabInterface());
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

void VerticalTabView::OnMouseMoved(const ui::MouseEvent& event) {
  // Hover state is handled by the parent if it is split.
  if (split_) {
    return;
  }

  // Linux enter/leave events are sometimes flaky, so we don't want to "miss"
  // an enter event and fail to hover the tab.
  UpdateHovered(true);
}

void VerticalTabView::OnMouseEntered(const ui::MouseEvent& event) {
  CHECK(collection_node_);
  UpdateHoverCard(this, TabSlotController::HoverCardUpdateType::kHover);

  // Hover state is handled by the parent if it is split.
  if (split_) {
    return;
  }

  UpdateHovered(true);
}

void VerticalTabView::OnMouseExited(const ui::MouseEvent& event) {
  CHECK(collection_node_);

  // Hover state is handled by the parent if it is split.
  if (split_) {
    return;
  }

  UpdateHovered(false);
}

bool VerticalTabView::OnMouseDragged(const ui::MouseEvent& event) {
  // Protect against key presses when the tab is animating out. Drag events may
  // call this function after the node has been deleted.
  if (!collection_node_) {
    return false;
  }

  auto* controller = collection_node_->GetController();
  CHECK(controller);
  return controller->GetDragHandler().ContinueDrag(*this, event);
}

void VerticalTabView::OnGestureEvent(ui::GestureEvent* event) {
  CHECK(collection_node_);
  UpdateHoverCard(nullptr, TabSlotController::HoverCardUpdateType::kEvent);

  switch (event->type()) {
    case ui::EventType::kGestureTapDown: {
      auto* controller = collection_node_->GetController();
      CHECK(controller);
      // TAP_DOWN is only dispatched for the first touch point.
      CHECK_EQ(1, event->details().touch_points());
      if (!selected_) {
        controller->SelectTab(GetTabInterface(), GetGestureDetail(*event));
      }
      event->SetHandled();
      break;
    }

    default:
      break;
  }
}

void VerticalTabView::OnPaint(gfx::Canvas* canvas) {
  // Split pinned tabs have a merged background that is rendered in
  // `VerticalSplitTabView`.
  if (pinned_ && split_) {
    return;
  }

  if (active_tab_fill_id_.has_value() || inactive_tab_fill_id_.has_value()) {
    PaintTabBackgroundWithImages(canvas, active_tab_fill_id_,
                                 inactive_tab_fill_id_);
  } else {
    PaintTabBackgroundFill(canvas, GetSelectionState(),
                           IsHoverAnimationActive(), std::nullopt);
  }

  views::View::OnPaint(canvas);
}

void VerticalTabView::PaintTabBackgroundWithImages(
    gfx::Canvas* canvas,
    std::optional<int> active_tab_fill_id,
    std::optional<int> inactive_tab_fill_id) const {
  const TabStyle::TabSelectionState current_state = GetSelectionState();

  if (current_state == TabStyle::TabSelectionState::kActive) {
    PaintTabBackgroundFill(canvas, TabStyle::TabSelectionState::kActive,
                           /*hovered=*/false, active_tab_fill_id);
  } else {
    PaintTabBackgroundFill(canvas, TabStyle::TabSelectionState::kInactive,
                           /*hovered=*/false, inactive_tab_fill_id);

    const float opacity = GetCurrentActiveOpacity();
    if (opacity > 0) {
      canvas->SaveLayerAlpha(base::ClampRound<uint8_t>(opacity * 0xff),
                             GetLocalBounds());
      PaintTabBackgroundFill(canvas, TabStyle::TabSelectionState::kActive,
                             /*hovered=*/false, active_tab_fill_id);
      canvas->Restore();
    }
  }
}

float VerticalTabView::GetCurrentActiveOpacity() const {
  const TabStyle::TabSelectionState selection_state = GetSelectionState();
  if (selection_state == TabStyle::TabSelectionState::kActive) {
    return 1.0f;
  }
  const float base_opacity =
      selection_state == TabStyle::TabSelectionState::kSelected
          ? tab_style()->GetSelectedTabOpacity()
          : 0.0f;
  if (!IsHoverAnimationActive()) {
    return base_opacity;
  }
  return std::lerp(base_opacity, GetHoverOpacity(), GetHoverAnimationValue());
}

void VerticalTabView::PaintTabBackgroundFill(
    gfx::Canvas* canvas,
    TabStyle::TabSelectionState selection_state,
    bool hovered,
    std::optional<int> fill_id) const {
  if (ShouldPaintTabBackgroundColor(selection_state, fill_id.has_value(),
                                    hovered)) {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(tab_style_->GetCurrentTabBackgroundColor(
        GetSelectionState(), IsHoverAnimationActive(), GetHoverAnimationValue(),
        IsFrameActive(), GetColorProvider()));
    canvas->DrawRect(GetContentsBounds(), flags);
  }

  if (fill_id.has_value()) {
    gfx::ImageSkia* image =
        GetThemeProvider()->GetImageSkiaNamed(fill_id.value());
    canvas->TileImageInt(*image, 0, 0, 0, 0, width(), height());
  }
}

bool VerticalTabView::ShouldPaintTabBackgroundColor(
    TabStyle::TabSelectionState selection_state,
    bool has_custom_background,
    bool hovered) const {
  if (selection_state == TabStyle::TabSelectionState::kActive) {
    return true;
  }

  if (has_custom_background) {
    return false;
  }

  if (hovered) {
    return true;
  }

  return GetThemeProvider()->GetDisplayProperty(
      ThemeProperties::SHOULD_FILL_BACKGROUND_TAB_COLOR);
}

void VerticalTabView::AddedToWidget() {
  paint_as_active_subscription_ =
      GetWidget()->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &VerticalTabView::UpdateColors, base::Unretained(this)));

  OnDataChanged();

  // Recompute accessible name when the structure changes.
  UpdateAccessibleName();

  // Recompute the hovered state as mouse events are not processed if a view
  // removed from the widget and added.
  if (!split_) {
    UpdateHovered(IsMouseHovered());
  }
}

void VerticalTabView::RemovedFromWidget() {
  paint_as_active_subscription_ = {};
  UpdateHovered(false);
}

void VerticalTabView::OnFocus() {
  views::View::OnFocus();

  if (collection_node_ && collection_node_->GetController()) {
    collection_node_->GetController()->TabKeyboardFocusChangedTo(
        GetTabInterface());
  }
}

void VerticalTabView::OnBlur() {
  views::View::OnBlur();

  if (collection_node_ && collection_node_->GetController()) {
    collection_node_->GetController()->TabKeyboardFocusChangedTo(nullptr);
  }
}

void VerticalTabView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  SetClipPath(GetPath());
}

void VerticalTabView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateThemeColors();
  UpdateColors();
}

gfx::Rect VerticalTabView::GetChildBounds(const gfx::Rect& container,
                                          const TabChildConfig& config,
                                          const bool center) const {
  int preferred_width;
  int preferred_height;
  if (config.expand) {
    preferred_width = container.width() - config.padding;
    // The only expandable view is the views::Label. Just get the line height to
    // make calculating bounds cheaper.
    CHECK(views::IsViewClass<views::Label>(config.view));
    preferred_height = static_cast<views::Label*>(config.view)->GetLineHeight();
  } else {
    const gfx::Size preferred_size = config.view->GetPreferredSize();
    preferred_width = preferred_size.width();
    preferred_height = preferred_size.height();
  }

  // Some icons have larger sizes to account for decoration. Make a distinction
  // between the design width and the actual width.
  const int design_width =
      config.expand ? container.width() - config.padding : config.min_width;

  int x = container.x();
  if (center) {
    x += 0.5 * (container.width() - preferred_width);
  } else if (config.align_leading) {
    x += 0.5 * (design_width - preferred_width);
  } else {
    x += container.width() - 0.5 * (design_width + preferred_width);
  }
  const int y = container.y() + 0.5 * (container.height() - preferred_height);

  return gfx::Rect(x, y, preferred_width, preferred_height);
}

absl::flat_hash_map<views::View*, bool>
VerticalTabView::CalculateChildVisibilities() const {
  absl::flat_hash_map<views::View*, bool> child_visibility_map;

  child_visibility_map[title_] = !pinned_;

  child_visibility_map[alert_indicator_] =
      alert_indicator_->showing_alert_state().has_value();
#if BUILDFLAG(ENABLE_GLIC)
  if (glic_tab_underline_view_ && (alert_indicator_->showing_alert_state() ==
                                       tabs::TabAlert::kGlicAccessing ||
                                   alert_indicator_->showing_alert_state() ==
                                       tabs::TabAlert::kGlicSharing)) {
    child_visibility_map[alert_indicator_] = false;
  }
#endif

  child_visibility_map[icon_] =
      !pinned_ || !child_visibility_map[alert_indicator_];

  if (pinned_) {
    child_visibility_map[close_button_] = false;
  } else if (active_) {
    child_visibility_map[close_button_] = true;
  } else if (collapsed_) {
    child_visibility_map[close_button_] = false;
  } else {
    child_visibility_map[close_button_] = hovered_;
  }

  return child_visibility_map;
}

views::ProposedLayout VerticalTabView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  auto child_visibility_map = CalculateChildVisibilities();

  const int width = size_bounds.width().value_or(
      VerticalTabStripRegionView::kUncollapsedMaxWidth);
  const int height =
      GetLayoutConstant(pinned_ ? LayoutConstant::kVerticalTabPinnedHeight
                                : LayoutConstant::kVerticalTabHeight);

  views::ProposedLayout layouts;
  layouts.host_size = gfx::Size(width, height);

  gfx::Rect bounds_remaining = gfx::Rect(0, 0, width, height);
  bounds_remaining.Inset(gfx::Insets::VH(0, kHorizontalInset));

  // If the tab is collapsed but animating with a wider width then we shouldn't
  // center the contents.
  const bool is_centered =
      (collapsed_ && width < VerticalTabStripRegionView::kCollapsedWidth) ||
      pinned_;

  int placed_children = 0;
  for (const auto& child : tab_children_configs_) {
    const bool can_render_child =
        is_centered
            ? (placed_children == 0)
            : (child.min_width + child.padding < bounds_remaining.width() ||
               placed_children < 2);
    if (child_visibility_map[child.view] && can_render_child) {
      layouts.child_layouts.emplace_back(
          child.view.get(), child_visibility_map[child.view],
          GetChildBounds(bounds_remaining, child, is_centered));

      if (!is_centered) {
        bounds_remaining.Inset(
            child.align_leading
                ? gfx::Insets().set_left(child.padding + child.min_width)
                : gfx::Insets().set_right(child.padding + child.min_width));
      }

      placed_children += 1;
    } else {
      layouts.child_layouts.emplace_back(
          child.view.get(), child_visibility_map[child.view],
          gfx::Rect(bounds_remaining.x(), bounds_remaining.y(), 0, 0));
    }
  }

  return layouts;
}

bool VerticalTabView::GetHitTestMask(SkPath* mask) const {
  *mask = GetPath();
  return true;
}

bool VerticalTabView::ShouldEnableMuteToggle(int required_width) {
  // TODO(crbug.com/454686636): Determine if there is enough space to activate
  // the tab in collapsed, pinned, or split states.
  return true;
}

void VerticalTabView::ToggleTabAudioMute() {
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

bool VerticalTabView::IsApparentlyActive() const {
  if (active_) {
    return true;
  }
  if (hovered_) {
    return GetHoverOpacity() > 0.5f;
  }
  return selected_;
}

void VerticalTabView::AlertStateChanged() {
  // TODO(crbug.com/457525548): Update hover card.
  InvalidateLayout();
}

void VerticalTabView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  if (collection_node_) {
    if (auto* controller = collection_node_->GetController()) {
      controller->ShowContextMenuForNode(collection_node_, source, point,
                                         source_type);
    }
  }
}

bool VerticalTabView::IsActive() const {
  return active_;
}

bool VerticalTabView::IsValid() const {
  return collection_node_ && !IsDragging();
}

const TabRendererData& VerticalTabView::data() const {
  return tab_data_;
}

views::BubbleBorder::Arrow VerticalTabView::GetAnchorPosition() const {
  if (pinned_ && !collapsed_) {
    return views::BubbleBorder::Arrow::TOP_LEFT;
  }
  return views::BubbleBorder::Arrow::LEFT_TOP;
}

void VerticalTabView::ResetCollectionNode() {
  CHECK(collection_node_);
  TabHoverCardController* hover_card_controller =
      collection_node_->GetController()->GetHoverCardController();
  if (hover_card_controller &&
      hover_card_controller->IsHoverCardShowingForTab(this)) {
    hover_card_controller->UpdateHoverCard(
        nullptr, TabSlotController::HoverCardUpdateType::kTabRemoved);
  }

  // Reset the active/selected/hovered states so the tab animates out without a
  // background.
  active_ = false;
  selected_ = false;

  // Update the callbacks for the buttons so that we dont call anything that
  // needs the node.
  close_button_->SetCallback(base::RepeatingClosure(base::DoNothing()));

  collection_node_ = nullptr;
}

void VerticalTabView::UpdateAccessibleName() {
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

void VerticalTabView::OnAXNameChanged(ax::mojom::StringAttribute attribute,
                                      const std::optional<std::string>& name) {
  if (GetWidget() && active_) {
    GetWidget()->UpdateAccessibleNameForRootView();
  }
}

void VerticalTabView::OnCollapsedStateChanged(
    tabs::VerticalTabStripStateController* controller) {
  collapsed_ = controller->IsCollapsed();
}

void VerticalTabView::OnDataChanged() {
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

  UpdateColors();
  InvalidateLayout();
}

void VerticalTabView::SetSelection(bool selected) {
  if (selected_ == selected) {
    return;
  }

  selected_ = selected;
  GetViewAccessibility().SetIsSelected(selected_);
}

void VerticalTabView::UpdateTabData(tabs::TabInterface* tab) {
  TabRendererData new_data = TabRendererData::FromTabInterface(tab);
  TabRendererData old_data = std::move(tab_data_);
  tab_data_ = std::move(new_data);

  if (tabs::ShouldUpdateAccessibleName(old_data, tab_data_)) {
    UpdateAccessibleName();
  }

  icon_->SetData(tab_data_);
  icon_->SetActiveState(tab->IsActivated());
  icon_->SetAttention(TabIcon::AttentionType::kBlockedWebContents,
                      !tab->IsActivated() && tab->IsBlocked());
  icon_->SetAttention(TabIcon::AttentionType::kTabWantsAttentionStatus,
                      tab_data_.needs_attention);

  UpdateTitle();

  alert_indicator_->TransitionToAlertState(
      tabs::TabAlertController::GetAlertStateToShow(tab_data_.alert_state));
  alert_indicator_->UpdateEnabledForMuteToggle();
}

void VerticalTabView::UpdateTitle() {
  std::u16string title = tab_data_.title;
  if (tab_data_.should_render_loading_title) {
    title = icon_->GetShowingLoadingAnimation()
                ? l10n_util::GetStringUTF16(IDS_TAB_LOADING_TITLE)
                : CoreTabHelper::GetDefaultTitle();
  } else {
    title = Browser::FormatTitleForDisplay(title);
  }
  title_->SetText(title);
}

void VerticalTabView::UpdateBorder() {
  if (pinned_) {
    if (split_) {
      // Insets for border handled by the `VerticalSplitTabView`.
      SetBorder(views::CreateEmptyBorder(gfx::Insets(GetLayoutConstant(
          LayoutConstant::kVerticalTabPinnedBorderThickness))));
    } else {
      SetBorder(views::CreateRoundedRectBorder(
          GetLayoutConstant(LayoutConstant::kVerticalTabPinnedBorderThickness),
          GetLayoutConstant(LayoutConstant::kVerticalTabCornerRadius),
          IsFrameActive() ? kColorTabDividerFrameActive
                          : kColorTabDividerFrameInactive));
    }
  } else if (GetBorder()) {
    SetBorder(nullptr);
  }
}

void VerticalTabView::UpdateThemeColors() {
  if (!collection_node_) {
    return;
  }

  std::optional<int> active_tab_fill_id;
  if (GetThemeProvider()->HasCustomImage(IDR_THEME_TOOLBAR)) {
    active_tab_fill_id = IDR_THEME_TOOLBAR;
  }

  const tabs::TabInterface* tab_interface = GetTabInterface();
  if (!tab_interface) {
    return;
  }

  BrowserFrameView* const browser_frame_view =
      BrowserView::GetBrowserViewForBrowser(
          tab_interface->GetBrowserWindowInterface())
          ->browser_widget()
          ->GetFrameView();
  const std::optional<int> inactive_tab_fill_id =
      browser_frame_view ? browser_frame_view->GetCustomBackgroundId(
                               BrowserFrameActiveState::kUseCurrent)
                         : std::nullopt;

  active_tab_fill_id_ = active_tab_fill_id;
  inactive_tab_fill_id_ = inactive_tab_fill_id;
}

void VerticalTabView::UpdateColors() {
  UpdateContrastRatioValues();
  TabStyle::TabColors colors = tab_style_->CalculateTargetColors(
      GetSelectionState(), IsApparentlyActive(), hovered_, IsFrameActive(),
      GetColorProvider());
  title_->SetEnabledColor(colors.foreground_color);
  close_button_->SetColors(colors);
  alert_indicator_->OnParentTabButtonColorChanged();

  UpdateBorder();

  // TODO(crbug.com/465159185): Update focus ring colors.
  SchedulePaint();
}

void VerticalTabView::UpdateContrastRatioValues() {
  auto [hover_opacity_min, hover_opacity_max, radial_highlight_opacity, _] =
      tab_style_->GetContrastRatioValues(IsFrameActive(), GetColorProvider());
  hover_opacity_min_ = hover_opacity_min;
  hover_opacity_max_ = hover_opacity_max;
  radial_highlight_opacity_ = radial_highlight_opacity;
}

void VerticalTabView::CloseButtonPressed(const ui::Event& event) {
  CHECK(collection_node_);

  if (active_) {
    base::RecordAction(base::UserMetricsAction("CloseTab_Active"));
  } else {
    base::RecordAction(base::UserMetricsAction("CloseTab_Inactive"));
  }

  CHECK(alert_indicator_);
  if (!alert_indicator_->GetVisible()) {
    base::RecordAction(base::UserMetricsAction("CloseTab_NoAlertIndicator"));
  } else if (auto alert_state = tabs::TabAlertController::GetAlertStateToShow(
                 tab_data_.alert_state);
             alert_state.has_value()) {
    tabs::TabAlertController::RecordCloseTabMetrics(alert_state.value());
  }

  if (split_) {
    auto* split_view = views::AsViewClass<VerticalSplitTabView>(parent());
    base::RecordAction(base::UserMetricsAction(this == split_view->children()[0]
                                                   ? "CloseTab_StartTabInSplit"
                                                   : "CloseTab_EndTabInSplit"));
  }

  // Hide the interactive close button while the tab is animating out.
  if (close_button_) {
    close_button_->SetVisible(false);
  }

  collection_node_->GetController()->CloseTab(GetTabInterface());
}

void VerticalTabView::RecordMousePressedInTab() {
  views::View* parent_view = parent();
  while (parent_view &&
         !views::IsViewClass<VerticalTabStripView>(parent_view)) {
    parent_view = parent_view->parent();
  }

  auto* tab_strip_view = views::AsViewClass<VerticalTabStripView>(parent_view);
  CHECK(tab_strip_view);
  tab_strip_view->RecordMousePressedInTab();
}

double VerticalTabView::GetHoverAnimationValue() const {
  if (split_) {
    if (auto* split_view = views::AsViewClass<VerticalSplitTabView>(parent())) {
      return split_view->GetHoverAnimationValue();
    }
  }
  return hover_controller_ ? hover_controller_->GetAnimationValue()
                           : (hovered_ ? 1.0 : 0.0);
}

float VerticalTabView::GetHoverOpacity() const {
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

bool VerticalTabView::IsFrameActive() const {
  return GetWidget() ? GetWidget()->ShouldPaintAsActive() : true;
}

TabStyle::TabSelectionState VerticalTabView::GetSelectionState() const {
  return active_ ? TabStyle::TabSelectionState::kActive
                 : (selected_ ? TabStyle::TabSelectionState::kSelected
                              : TabStyle::TabSelectionState::kInactive);
}

bool VerticalTabView::IsDragging() const {
  return collection_node_ && collection_node_->GetController() &&
         collection_node_->GetController()->GetDragHandler().IsViewDragging(
             *this);
}

const tabs::TabInterface* VerticalTabView::GetTabInterface() const {
  return std::get<const tabs::TabInterface*>(collection_node_->GetNodeData());
}

void VerticalTabView::UpdateHoverCard(HoverCardAnchorTarget* target,
                                      int hover_card_update_type) {
  CHECK(collection_node_);

  if (TabHoverCardController* hover_card_controller =
          collection_node_->GetController()->GetHoverCardController()) {
    hover_card_controller->UpdateHoverCard(
        target, static_cast<TabSlotController::HoverCardUpdateType>(
                    hover_card_update_type));
  }
}

BEGIN_METADATA(VerticalTabView)
END_METADATA
