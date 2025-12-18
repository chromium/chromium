// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/themes/theme_properties.h"
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
#include "components/browser_apis/tab_strip/tab_strip_api_data_model.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/favicon_size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int kAfterIconPadding = 8;
constexpr int kAfterTitlePadding = 4;
constexpr int kAfterAlertIndicatorPadding = 4;
constexpr int kTitleNoCloseButtonRightPadding = 11;
constexpr int kTitleHeight = 18;
constexpr int kTabMinWidth = 38;

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
    : collection_node_(collection_node),
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
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  SetPreferredSize(
      gfx::Size(kTabMinWidth, GetLayoutConstant(VERTICAL_TAB_HEIGHT)));

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

void VerticalTabView::OnMouseEntered(const ui::MouseEvent& event) {
  UpdateHovered(true);
}

void VerticalTabView::OnMouseExited(const ui::MouseEvent& event) {
  UpdateHovered(false);
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

void VerticalTabView::OnPaint(gfx::Canvas* canvas) {
  // TODO(crbug.com/465540287): Handle the theme's custom images for the toolbar
  // area/frame background. Also consider using views::Background to draw the
  // background if that is compatible with how we handle custom images, so that
  // we no longer have to override OnPaint.
  if (active_ || IsHoverAnimationActive() ||
      GetThemeProvider()->GetDisplayProperty(
          ThemeProperties::SHOULD_FILL_BACKGROUND_TAB_COLOR)) {
    canvas->ClipPath(GetPath(), true);
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(tab_style_->GetCurrentTabBackgroundColor(
        GetSelectionState(), IsHoverAnimationActive(), GetHoverAnimationValue(),
        IsFrameActive(), GetColorProvider()));
    gfx::Rect local_bounds = GetLocalBounds();
    local_bounds.Inset(GetTabInset());
    canvas->DrawRect(local_bounds, flags);
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

void VerticalTabView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateColors();
}

views::ProposedLayout VerticalTabView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  // TODO(crbug.com/444283717): Separate pinned and unpinned tabs.
  views::ProposedLayout layouts;

  // TODO(crbug.com/454686636): Handle collapsed state.
  const int width = std::max(kTabMinWidth, size_bounds.width().value_or(0));
  const int height = GetLayoutConstant(VERTICAL_TAB_HEIGHT);
  layouts.host_size = gfx::Size(width, height);

  const gfx::Rect contents_rect = GetContentsBounds();

  // TabIcon calculates its preferred size by starting with the favicon size,
  // and enlarging it to fit the attention indicator and discard ring. Use its
  // preferred size instead of gfx::kFaviconSize.
  gfx::Size icon_size = icon_->GetPreferredSize();
  const int icon_padding = (height - icon_size.height()) / 2;
  const gfx::Rect icon_bounds(contents_rect.x() + icon_padding,
                              contents_rect.y() + icon_padding,
                              icon_size.width(), icon_size.height());
  layouts.child_layouts.emplace_back(icon_.get(), icon_->GetVisible(),
                                     icon_bounds);

  gfx::Size close_button_size = close_button_->GetPreferredSize();
  const int close_button_padding = (height - close_button_size.height()) / 2;
  const gfx::Rect close_button_bounds(
      contents_rect.right() - close_button_padding - close_button_size.width(),
      contents_rect.y() + close_button_padding, close_button_size.width(),
      close_button_size.height());
  layouts.child_layouts.emplace_back(
      close_button_.get(), close_button_->GetVisible(), close_button_bounds);
  // The close button icon is centered within the close button.
  const int close_button_icon_x =
      close_button_bounds.x() +
      (close_button_bounds.width() - GetLayoutConstant(TAB_CLOSE_BUTTON_SIZE)) /
          2;

  gfx::Size alert_indicator_size = alert_indicator_->GetPreferredSize();
  const int alert_indicator_padding =
      (height - alert_indicator_size.height()) / 2;
  const int alert_indicator_right =
      close_button_->GetVisible()
          ? close_button_icon_x - kAfterAlertIndicatorPadding
          : contents_rect.right() - alert_indicator_padding;
  const gfx::Rect alert_indicator_bounds(
      alert_indicator_right - alert_indicator_size.width(),
      contents_rect.y() + alert_indicator_padding, alert_indicator_size.width(),
      alert_indicator_size.height());
  layouts.child_layouts.emplace_back(alert_indicator_.get(),
                                     alert_indicator_->GetVisible(),
                                     alert_indicator_bounds);

  // kAfterIconPadding is the space between the title and the favicon, however
  // icon_ has extra space for the attention indicator and discard ring, given
  // by its insets.
  const int title_bounds_x =
      icon_bounds.right() - icon_->GetInsets().right() + kAfterIconPadding;
  const int title_bounds_y = contents_rect.y() + (height - kTitleHeight) / 2;
  const int title_bounds_right =
      alert_indicator_->GetVisible()
          ? alert_indicator_bounds.x() - kAfterTitlePadding
      : close_button_->GetVisible()
          ? close_button_icon_x - kAfterTitlePadding
          : contents_rect.right() - kTitleNoCloseButtonRightPadding;
  const gfx::Rect title_bounds(title_bounds_x, title_bounds_y,
                               title_bounds_right - title_bounds_x,
                               kTitleHeight);
  layouts.child_layouts.emplace_back(title_.get(), title_->GetVisible(),
                                     title_bounds);
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

  active_ = tab->IsActivated();
  selected_ = tab->IsSelected();

  int index =
      tab->GetBrowserWindowInterface()->GetTabStripModel()->GetIndexOfTab(tab);
  TabRendererData tab_data = TabRendererData::FromTabInModel(
      tab->GetBrowserWindowInterface()->GetTabStripModel(), index);

  icon_->SetData(tab_data);
  icon_->SetActiveState(active_);
  icon_->SetAttention(TabIcon::AttentionType::kBlockedWebContents,
                      active_ && tab_data.blocked);

  title_->SetText(tab_data.title);
  title_->SetVisible(!tab->IsPinned());

  alert_indicator_->TransitionToAlertState(
      tabs::TabAlertController::GetAlertStateToShow(tab_data.alert_state));
  UpdateAlertIndicatorVisibility();
  UpdateCloseButtonVisibility();

  UpdateColors();
  InvalidateLayout();
}

void VerticalTabView::UpdateBorder() {
  const tabs::TabInterface* tab = GetTabInterface();
  if (tab && tab->IsPinned() && !tab->IsSplit()) {
    SetBorder(views::CreateRoundedRectBorder(
        GetLayoutConstant(VERTICAL_TAB_PINNED_BORDER_THICKNESS),
        GetLayoutConstant(VERTICAL_TAB_CORNER_RADIUS),
        IsFrameActive() ? kColorTabDividerFrameActive
                        : kColorTabDividerFrameInactive));
  } else {
    SetBorder(nullptr);
  }
}

void VerticalTabView::UpdateAlertIndicatorVisibility() {
  alert_indicator_->UpdateAlertIndicatorAnimation();
  alert_indicator_->SetVisible(
      alert_indicator_->showing_alert_state().has_value());
}

void VerticalTabView::UpdateCloseButtonVisibility() {
  close_button_->SetVisible((active_ || hovered_) &&
                            !GetTabInterface()->IsPinned());
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

  SchedulePaint();
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
  const int tab_inset = GetTabInset();
  const float corner_radius =
      GetLayoutConstant(VERTICAL_TAB_CORNER_RADIUS) - 2 * tab_inset;
  SkVector radius = {corner_radius, corner_radius};
  const SkVector radii[4] = {radius, radius, radius, radius};
  SkPathBuilder path;
  path.addRRect(SkRRect::MakeRectRadii(SkRect::MakeWH(width() - 2 * tab_inset,
                                                      height() - 2 * tab_inset),
                                       radii)
                    .makeOffset(tab_inset, tab_inset));
  return path.detach();
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

int VerticalTabView::GetTabInset() const {
  const tabs::TabInterface* tab = GetTabInterface();
  if (tab->IsPinned() && tab->IsSplit()) {
    return GetLayoutConstant(VERTICAL_TAB_PINNED_BORDER_THICKNESS);
  }
  return 0;
}

BEGIN_METADATA(VerticalTabView)
END_METADATA
