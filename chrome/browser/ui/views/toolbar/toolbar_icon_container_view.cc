// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "extensions/common/extension_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_observer.h"

ToolbarIconContainerView::RoundRectBorder::RoundRectBorder(views::View* parent)
    : parent_(parent) {
  layer_.set_delegate(this);
  layer_.SetFillsBoundsOpaquely(false);
  layer_.SetFillsBoundsCompletely(false);
  layer_.SetOpacity(0);
  layer_.SetVisible(true);
}

void ToolbarIconContainerView::RoundRectBorder::OnPaintLayer(
    const ui::PaintContext& context) {
  ui::PaintRecorder paint_recorder(context, layer_.size());
  gfx::Canvas* canvas = paint_recorder.canvas();

  const int radius = ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kMaximum, layer_.size());
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(1);
  flags.setColor(
      parent_->GetColorProvider()->GetColor(kColorToolbarIconContainerBorder));
  gfx::RectF rect(gfx::SizeF(layer_.size()));
  rect.Inset(0.5f);  // Pixel edges -> pixel centers.
  canvas->DrawRoundRect(rect, radius, flags);
}

void ToolbarIconContainerView::RoundRectBorder::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {}

// Watches for widget restore (or first show) and resets the animation so icons
// don't spuriously "animate in" when a window is shown or restored. See
// crbug.com/1106506 for more details.
//
// There is currently no signal that is consistent across platforms and
// accessible from the View hierarchy that can tell us if, e.g., a window has
// been restored from a minimized state. While we could theoretically plumb
// state changes through from NativeWidget, we can observe the specific set of
// cases we want by observing the size of the window.
//
// We *cannot* observe the size of the widget itself, as at least on Windows,
// minimizing a window does not set the widget to 0x0, but rather a small,
// Windows 3.1-esque tile (~160x28) and moves it to [-32000, -32000], so far off
// the screen it can't appear on any monitor.
//
// What we can observe is the widget's root view, which is (a) always present
// after the toolbar has been added to its widget and through its entire
// lifetime, and (b) is actually set to zero size when the window is zero size
// or minimized on Windows.
class ToolbarIconContainerView::WidgetRestoreObserver
    : public views::ViewObserver {
 public:
  explicit WidgetRestoreObserver(
      ToolbarIconContainerView* toolbar_icon_container_view)
      : toolbar_icon_container_view_(toolbar_icon_container_view) {
    scoped_observation_.Observe(
        toolbar_icon_container_view->GetWidget()->GetRootView());
  }

  void OnViewBoundsChanged(views::View* observed_view) override {
    const bool is_collapsed = observed_view->bounds().IsEmpty();
    if (is_collapsed != was_collapsed_) {
      was_collapsed_ = is_collapsed;
      if (!is_collapsed) {
        toolbar_icon_container_view_->GetAnimatingLayoutManager()
            ->ResetLayout();
      }
    }
  }

 private:
  bool was_collapsed_ = true;
  const raw_ptr<ToolbarIconContainerView> toolbar_icon_container_view_;
  base::ScopedObservation<views::View, views::ViewObserver> scoped_observation_{
      this};
};

ToolbarIconContainerView::ToolbarIconContainerView(
    bool uses_highlight,
    bool use_default_target_layout)
    : uses_highlight_(uses_highlight) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetFillsBoundsCompletely(false);
  AddLayerToRegion(border_.layer(), views::LayerRegion::kBelow);

  views::AnimatingLayoutManager* animating_layout =
      SetLayoutManager(std::make_unique<views::AnimatingLayoutManager>());
  animating_layout->SetBoundsAnimationMode(
      views::AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  animating_layout->SetDefaultFadeMode(
      views::AnimatingLayoutManager::FadeInOutMode::kSlideFromTrailingEdge);
  if (use_default_target_layout) {
    auto* flex_layout = animating_layout->SetTargetLayoutManager(
        std::make_unique<views::FlexLayout>());
    flex_layout->SetCollapseMargins(true)
        .SetIgnoreDefaultMainAxisMargins(true)
        .SetDefault(
            views::kMarginsKey,
            gfx::Insets::VH(0, GetLayoutConstant(TOOLBAR_ELEMENT_PADDING)));
  }
}

ToolbarIconContainerView::~ToolbarIconContainerView() {
  // As childred might be Observers of |this|, we need to destroy them before
  // destroying |observers_|.
  RemoveAllChildViews();
}

void ToolbarIconContainerView::AddMainItem(views::View* item) {
  DCHECK(!main_item_);
  main_item_ = item;
  auto* const main_button = views::Button::AsButton(item);
  if (main_button)
    ObserveButton(main_button);

  AddChildView(main_item_.get());
}

void ToolbarIconContainerView::ObserveButton(views::Button* button) {
  // We don't care about the main button being highlighted.
  if (button != main_item_) {
    subscriptions_.push_back(
        views::InkDrop::Get(button)->AddHighlightedChangedCallback(
            base::BindRepeating(
                &ToolbarIconContainerView::OnButtonHighlightedChanged,
                base::Unretained(this), base::Unretained(button))));
  }
  subscriptions_.push_back(button->AddStateChangedCallback(base::BindRepeating(
      &ToolbarIconContainerView::UpdateHighlight, base::Unretained(this))));
  button->AddObserver(this);
}

void ToolbarIconContainerView::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void ToolbarIconContainerView::RemoveObserver(const Observer* obs) {
  observers_.RemoveObserver(obs);
}

bool ToolbarIconContainerView::GetHighlighted() const {
  if (!uses_highlight_)
    return false;

  if (IsMouseHovered() && (!main_item_ || !main_item_->IsMouseHovered()))
    return true;

  // Focused, pressed or hovered children should trigger the highlight.
  for (const views::View* child : children()) {
    if (child == main_item_)
      continue;
    if (child->HasFocus())
      return true;
    const views::Button* button = views::Button::AsButton(child);
    if (!button)
      continue;
    if (button->GetState() == views::Button::ButtonState::STATE_PRESSED ||
        button->GetState() == views::Button::ButtonState::STATE_HOVERED) {
      return true;
    }
    // The container should also be highlighted if a dialog is anchored to.
    if (base::Contains(highlighted_buttons_, button))
      return true;
  }

  return false;
}

void ToolbarIconContainerView::OnThemeChanged() {
  views::View::OnThemeChanged();
  border_.layer()->SchedulePaint(GetLocalBounds());
}

void ToolbarIconContainerView::OnViewFocused(views::View* observed_view) {
  UpdateHighlight();
}

void ToolbarIconContainerView::OnViewBlurred(views::View* observed_view) {
  UpdateHighlight();
}

views::AnimatingLayoutManager*
ToolbarIconContainerView::GetAnimatingLayoutManager() {
  return static_cast<views::AnimatingLayoutManager*>(GetLayoutManager());
}

const views::AnimatingLayoutManager*
ToolbarIconContainerView::GetAnimatingLayoutManager() const {
  return static_cast<const views::AnimatingLayoutManager*>(GetLayoutManager());
}

views::FlexLayout* ToolbarIconContainerView::GetTargetLayoutManager() {
  return static_cast<views::FlexLayout*>(
      GetAnimatingLayoutManager()->target_layout_manager());
}

void ToolbarIconContainerView::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  const gfx::Rect bounds = GetLocalBounds();
  border_.layer()->SetBounds(ConvertRectToWidget(bounds));
  border_.layer()->SchedulePaint(bounds);
}

void ToolbarIconContainerView::OnMouseEntered(const ui::MouseEvent& event) {
  UpdateHighlight();
}

void ToolbarIconContainerView::OnMouseExited(const ui::MouseEvent& event) {
  UpdateHighlight();
}

void ToolbarIconContainerView::AddedToWidget() {
  // Add an observer to reset the animation if the browser window is restored,
  // preventing spurious animation. (See crbug.com/1106506)
  restore_observer_ = std::make_unique<WidgetRestoreObserver>(this);
}

void ToolbarIconContainerView::UpdateHighlight() {
  // New feature doesn't have a border around the toolbar icons.
  // TODO(crbug.com/40811196): Remove ToolbarIconContainerView once feature is
  // rolled out.
  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    return;
  }

  bool showing_before = border_.layer()->GetTargetOpacity() == 1;
  border_.layer()->SetOpacity(GetHighlighted() ? 1 : 0);

  if (showing_before == (border_.layer()->GetTargetOpacity() == 1))
    return;
  observers_.Notify(&Observer::OnHighlightChanged);
}

void ToolbarIconContainerView::OnButtonHighlightedChanged(
    views::Button* button) {
  if (views::InkDrop::Get(button)->GetHighlighted())
    highlighted_buttons_.insert(button);
  else
    highlighted_buttons_.erase(button);

  UpdateHighlight();
}

BEGIN_METADATA(ToolbarIconContainerView)
ADD_READONLY_PROPERTY_METADATA(bool, Highlighted)
END_METADATA
