// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/tab_muted_utils.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/alert_indicator_button.h"
#include "chrome/browser/ui/views/tabs/glow_hover_controller.h"
#include "chrome/browser/ui/views/tabs/tab_close_button.h"
#include "chrome/browser/ui/views/tabs/tab_icon.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_drag_handler.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_controller.h"
#include "chrome/common/buildflags.h"
#include "components/browser_apis/tab_strip/tab_strip_api_data_model.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
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
// The padding should be 8dp but the icon contains 3dp of padding on both sides.
constexpr int kIconPadding = 5;
constexpr int kAfterTitlePadding = 2;
constexpr int kCloseButtonLeftPadding = -1;
constexpr int kCloseButtonRightPadding = 2;
constexpr int kAfterAlertIndicatorPadding = 4;
constexpr int kTitleMinWidth = 10;

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

  gfx::Size GetMinimumSize() const override {
    return gfx::Size(kTitleMinWidth, GetLineHeight());
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
    : collection_node_(collection_node),
      flex_layout_(SetLayoutManager(std::make_unique<views::FlexLayout>())),
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
                  GetLayoutConstant(VERTICAL_TAB_HEIGHT)));
  }
#endif

  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  flex_layout_->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCollapseMargins(true)
      .SetMinimumCrossAxisSize(GetLayoutConstant(VERTICAL_TAB_HEIGHT));

  alert_indicator_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferredSnapToZero,
                               views::MaximumFlexSizeRule::kPreferred)
          .WithOrder(2));
  alert_indicator_->SetProperty(
      views::kMarginsKey, gfx::Insets().set_right(kAfterAlertIndicatorPadding));
  icon_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferredSnapToZero,
                               views::MaximumFlexSizeRule::kPreferred)
          .WithOrder(3));
  icon_->SetProperty(views::kMarginsKey,
                     gfx::Insets().set_left_right(kIconPadding, kIconPadding));

  title_->SetProperty(views::kFlexBehaviorKey,
                      views::FlexSpecification(
                          views::MinimumFlexSizeRule::kScaleToMinimumSnapToZero,
                          views::MaximumFlexSizeRule::kUnbounded)
                          .WithOrder(4));
  title_->SetProperty(views::kMarginsKey,
                      gfx::Insets().set_right(kAfterTitlePadding));

  close_button_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferredSnapToZero,
                               views::MaximumFlexSizeRule::kPreferred)
          .WithOrder(1));
  close_button_->SetProperty(views::kMarginsKey, gfx::Insets().set_left_right(
                                                     kCloseButtonLeftPadding,
                                                     kCloseButtonRightPadding));

  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  // So we get don't get enter/exit on children and don't prematurely stop the
  // hover.
  SetNotifyEnterExitOnChild(true);

  SetFocusBehavior(FocusBehavior::ALWAYS);
  views::FocusRing::Install(this);

  GetViewAccessibility().SetRole(ax::mojom::Role::kTab);
  GetViewAccessibility().SetName(
      std::string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);

  node_destroyed_subscription_ =
      collection_node_->RegisterWillDestroyCallback(base::BindOnce(
          &VerticalTabView::ResetCollectionNode, base::Unretained(this)));
  data_changed_subscription_ =
      collection_node_->RegisterDataChangedCallback(base::BindRepeating(
          &VerticalTabView::OnDataChanged, base::Unretained(this)));

  OnDataChanged();

  set_context_menu_controller(this);
}

VerticalTabView::~VerticalTabView() = default;

void VerticalTabView::StepLoadingAnimation(
    const base::TimeDelta& elapsed_time) {
  // TODO(crbug.com/467710547): Paint favicon to a layer when tab strip isn't
  // animating or when dragging isn't in progress or in full screen mode.
  icon_->StepLoadingAnimation(elapsed_time);
}

void VerticalTabView::UpdateHovered(bool hovered) {
  if (hovered_ == hovered) {
    return;
  }

  hovered_ = hovered;
  if (hover_controller_) {
    if (hovered_) {
      hover_controller_->SetSubtleOpacityScale(radial_highlight_opacity_);
      hover_controller_->Show(TabStyle::ShowHoverStyle::kSubtle);
    } else {
      hover_controller_->Hide(TabStyle::HideHoverStyle::kGradual);
    }
  }
  UpdateColors();
  UpdateCloseButtonVisibility();
}

void VerticalTabView::OnTabDragOver() {
  auto* controller = collection_node_->GetController();
  CHECK(controller);
  controller->GetDragHandler().DraggedTabsOverNode(*collection_node_);
}

bool VerticalTabView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_RETURN && selected_) {
    collection_node_->GetController()->SelectTab(GetTabInterface(),
                                                 GetGestureDetail(event));
    return true;
  }
  return false;
}

bool VerticalTabView::OnKeyReleased(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE && selected_) {
    collection_node_->GetController()->SelectTab(GetTabInterface(),
                                                 GetGestureDetail(event));
    return true;
  }
  return false;
}

bool VerticalTabView::OnMousePressed(const ui::MouseEvent& event) {
  auto* controller = collection_node_->GetController();
  shift_pressed_on_mouse_down_ = event.IsShiftDown();

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
    }
    // Potentially start the drag for the mouse press.
    // Follow-up mouse-movement events will update the drag controller and
    // eventually kick off the drag-loop.
    controller->GetDragHandler().InitializeDrag(*collection_node_, event);
  }
  return true;
}

void VerticalTabView::OnMouseReleased(const ui::MouseEvent& event) {
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
  // Linux enter/leave events are sometimes flaky, so we don't want to "miss"
  // an enter event and fail to hover the tab.
  UpdateHovered(true);
}

bool VerticalTabView::OnMouseDragged(const ui::MouseEvent& event) {
  auto* controller = collection_node_->GetController();
  CHECK(controller);
  return controller->GetDragHandler().ContinueDrag(*this, event);
}

void VerticalTabView::OnMouseEntered(const ui::MouseEvent& event) {
  UpdateHovered(true);
}

void VerticalTabView::OnMouseExited(const ui::MouseEvent& event) {
  UpdateHovered(false);
}

void VerticalTabView::OnPaint(gfx::Canvas* canvas) {
  // TODO(crbug.com/465540287): Handle the theme's custom images for the toolbar
  // area/frame background. Also consider using views::Background to draw the
  // background if that is compatible with how we handle custom images, so that
  // we no longer have to override OnPaint.
  if (active_ || IsHoverAnimationActive() ||
      GetThemeProvider()->GetDisplayProperty(
          ThemeProperties::SHOULD_FILL_BACKGROUND_TAB_COLOR)) {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(tab_style_->GetCurrentTabBackgroundColor(
        GetSelectionState(), IsHoverAnimationActive(), GetHoverAnimationValue(),
        IsFrameActive(), GetColorProvider()));
    canvas->DrawRect(GetContentsBounds(), flags);
  }

  views::View::OnPaint(canvas);
}

void VerticalTabView::AddedToWidget() {
  paint_as_active_subscription_ =
      GetWidget()->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &VerticalTabView::UpdateColors, base::Unretained(this)));
}

void VerticalTabView::RemovedFromWidget() {
  paint_as_active_subscription_ = {};
}

void VerticalTabView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  SetClipPath(GetPath());
}

void VerticalTabView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateColors();
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
  content::WebContents* const contents = GetTabInterface()->GetContents();
  bool mute = !contents->IsAudioMuted();
  // TODO(crbug.com/468033457): Log tab audio muted metric.
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
  UpdateAlertIndicatorVisibility();
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

void VerticalTabView::ResetCollectionNode() {
  collection_node_ = nullptr;
}

void VerticalTabView::OnDataChanged() {
  const tabs::TabInterface* tab = GetTabInterface();
  CHECK(tab);

  // TODO(crbug.com/470155950): Ensure proper observations for updates.
  active_ = tab->IsVisible();
  selected_ = tab->IsSelected();
  split_ = tab->IsSplit();
  pinned_ = tab->IsPinned();

  int index =
      tab->GetBrowserWindowInterface()->GetTabStripModel()->GetIndexOfTab(tab);
  TabRendererData tab_data = TabRendererData::FromTabInModel(
      tab->GetBrowserWindowInterface()->GetTabStripModel(), index);

  icon_->SetData(tab_data);
  icon_->SetActiveState(tab->IsActivated());
  icon_->SetAttention(TabIcon::AttentionType::kBlockedWebContents,
                      tab->IsActivated() && tab_data.blocked);
  icon_->SetProperty(
      views::kMarginsKey,
      pinned_ ? gfx::Insets()
              : gfx::Insets().set_left_right(kIconPadding, kIconPadding));

  title_->SetText(tab_data.title);
  title_->SetVisible(!pinned_);

  alert_indicator_->TransitionToAlertState(
      tabs::TabAlertController::GetAlertStateToShow(tab_data.alert_state));
  alert_indicator_->SetProperty(
      views::kMarginsKey,
      pinned_ ? gfx::Insets()
              : gfx::Insets().set_right(kAfterAlertIndicatorPadding));
  UpdateAlertIndicatorVisibility();
  UpdateCloseButtonVisibility();

  flex_layout_->SetMainAxisAlignment(pinned_ ? views::LayoutAlignment::kCenter
                                             : views::LayoutAlignment::kStart);

  UpdateColors();
  InvalidateLayout();
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

void VerticalTabView::UpdateAlertIndicatorVisibility() {
  alert_indicator_->UpdateAlertIndicatorAnimation();
  bool alert_indicator_visible =
      alert_indicator_->showing_alert_state().has_value();

#if BUILDFLAG(ENABLE_GLIC)
  if (glic_tab_underline_view_ && (alert_indicator_->showing_alert_state() ==
                                       tabs::TabAlert::kGlicAccessing ||
                                   alert_indicator_->showing_alert_state() ==
                                       tabs::TabAlert::kGlicSharing)) {
    alert_indicator_visible = false;
  }
#endif
  alert_indicator_->SetVisible(alert_indicator_visible);
  icon_->SetVisible(!pinned_ || !alert_indicator_visible);
}

void VerticalTabView::UpdateCloseButtonVisibility() {
  close_button_->SetVisible((active_ || hovered_) && !pinned_);
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
  // TODO(crbug.com/467735166): Log tab closing UMAs.
  collection_node_->GetController()->CloseTab(GetTabInterface());
}

bool VerticalTabView::IsHoverAnimationActive() const {
  return hovered_ || (hover_controller_ && hover_controller_->ShouldDraw());
}

double VerticalTabView::GetHoverAnimationValue() const {
  if (!hover_controller_) {
    return hovered_ ? 1.0 : 0.0;
  }
  return hover_controller_->GetAnimationValue();
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

SkPath VerticalTabView::GetPath() const {
  const SkScalar corner_radius = SkIntToScalar(
      GetLayoutConstant(LayoutConstant::kVerticalTabCornerRadius) +
      (split_ ? GetInsets().height() : 0));
  return SkPath::RRect(SkRRect::MakeRectXY(gfx::RectToSkRect(GetLocalBounds()),
                                           corner_radius, corner_radius));
}

bool VerticalTabView::IsFrameActive() const {
  return GetWidget() ? GetWidget()->ShouldPaintAsActive() : true;
}

TabStyle::TabSelectionState VerticalTabView::GetSelectionState() const {
  return active_ ? TabStyle::TabSelectionState::kActive
                 : (selected_ ? TabStyle::TabSelectionState::kSelected
                              : TabStyle::TabSelectionState::kInactive);
}

const tabs::TabInterface* VerticalTabView::GetTabInterface() const {
  return std::get<const tabs::TabInterface*>(collection_node_->GetNodeData());
}

BEGIN_METADATA(VerticalTabView)
END_METADATA
