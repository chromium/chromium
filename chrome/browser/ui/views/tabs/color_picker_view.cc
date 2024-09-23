// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/color_picker_view.h"

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/tab_groups/tab_group_color.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"

namespace {

class ColorPickerHighlightPathGenerator : public views::HighlightPathGenerator {
 public:
  ColorPickerHighlightPathGenerator() = default;
  ColorPickerHighlightPathGenerator(const ColorPickerHighlightPathGenerator&) =
      delete;
  ColorPickerHighlightPathGenerator& operator=(
      const ColorPickerHighlightPathGenerator&) = delete;

  // views::HighlightPathGenerator:
  SkPath GetHighlightPath(const views::View* view) override {
    // Our highlight path should be slightly larger than the circle we paint.
    gfx::RectF bounds(view->GetContentsBounds());
    bounds.Inset(-2.0f);
    const gfx::PointF center = bounds.CenterPoint();
    return SkPath().addCircle(center.x(), center.y(), bounds.width() / 2.0f);
  }
};

}  // namespace

// Represents one of the colors the user can pick from. Displayed as a solid
// circle of the given color.
class ColorPickerElementView : public views::Button {
  METADATA_HEADER(ColorPickerElementView, views::Button)

 public:
  ColorPickerElementView(
      base::RepeatingCallback<void(ColorPickerElementView*)> selected_callback,
      const views::BubbleDialogDelegateView* bubble_view,
      tab_groups::TabGroupColorId color_id,
      std::u16string color_name)
      : Button(base::BindRepeating(&ColorPickerElementView::ButtonPressed,
                                   base::Unretained(this))),
        selected_callback_(std::move(selected_callback)),
        bubble_view_(bubble_view),
        color_id_(color_id),
        color_name_(color_name) {
    DCHECK(selected_callback_);

    GetViewAccessibility().SetName(color_name);
    SetInstallFocusRingOnFocus(true);
    views::HighlightPathGenerator::Install(
        this, std::make_unique<ColorPickerHighlightPathGenerator>());

    // When calculating padding, halve the value because color elements are
    // displayed side-by-side and each contribute half the spacing between them.
    const int padding = ChromeLayoutProvider::Get()->GetDistanceMetric(
                            views::DISTANCE_RELATED_BUTTON_HORIZONTAL) /
                        2;
    // The padding of the color element circle is adaptive, to improve the hit
    // target size on touch devices.
    gfx::Insets insets = ui::TouchUiController::Get()->touch_ui()
                             ? gfx::Insets(padding * 2)
                             : gfx::Insets(padding);
    SetBorder(views::CreateEmptyBorder(insets));

    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);
    SetAnimateOnStateChange(true);
    GetViewAccessibility().SetRole(ax::mojom::Role::kRadioButton);
    GetViewAccessibility().SetCheckedState(
        selected_ ? ax::mojom::CheckedState::kTrue
                  : ax::mojom::CheckedState::kFalse);
  }

  ~ColorPickerElementView() override = default;

  void SetSelected(bool selected) {
    if (selected_ == selected) {
      return;
    }
    selected_ = selected;
    GetViewAccessibility().SetCheckedState(
        selected_ ? ax::mojom::CheckedState::kTrue
                  : ax::mojom::CheckedState::kFalse);
    SchedulePaint();
  }

  bool GetSelected() const { return selected_; }

  // views::Button:
  bool IsGroupFocusTraversable() const override {
    // Tab should only focus the selected element.
    return false;
  }

  views::View* GetSelectedViewForGroup(int group) override {
    DCHECK(parent());
    return parent()->GetSelectedViewForGroup(group);
  }

  std::u16string GetTooltipText(const gfx::Point& p) const override {
    return color_name_;
  }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    const gfx::Insets insets = GetInsets();
    // The size of the color element circle is adaptive, to improve the hit
    // target size on touch devices.
    const int circle_size = ui::TouchUiController::Get()->touch_ui()
                                ? 3 * gfx::kFaviconSize / 2
                                : gfx::kFaviconSize;
    gfx::Size size(circle_size, circle_size);
    size.Enlarge(insets.width(), insets.height());
    return size;
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    // Paint a colored circle surrounded by a bit of empty space.
    gfx::RectF bounds(GetContentsBounds());

    // We should be a circle.
    DCHECK_EQ(bounds.width(), bounds.height());

    const SkColor color =
        GetColorProvider()->GetColor(GetTabGroupDialogColorId(color_id_));

    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(color);
    flags.setAntiAlias(true);
    canvas->DrawCircle(bounds.CenterPoint(), bounds.width() / 2.0f, flags);

    PaintSelectionIndicator(canvas);
  }

 private:
  // Paints a ring in our color circle to indicate selection or mouse hover.
  // Does nothing if not selected or hovered.
  void PaintSelectionIndicator(gfx::Canvas* canvas) {
    if (!selected_) {
      return;
    }

    // Visual parameters of our ring.
    constexpr float kInset = 3.0f;
    constexpr float kThickness = 2.0f;
    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(kThickness);
    flags.setAntiAlias(true);
    flags.setColor(bubble_view_->color());

    gfx::RectF indicator_bounds(GetContentsBounds());
    indicator_bounds.Inset(gfx::InsetsF(kInset));
    DCHECK(!indicator_bounds.size().IsEmpty());
    canvas->DrawCircle(indicator_bounds.CenterPoint(),
                       indicator_bounds.width() / 2.0f, flags);
  }

  void ButtonPressed() {
    // Pressing this a second time shouldn't do anything.
    if (!selected_) {
      SetSelected(true);
      SchedulePaint();
      selected_callback_.Run(this);
    }
  }

  const base::RepeatingCallback<void(ColorPickerElementView*)>
      selected_callback_;
  raw_ptr<const views::BubbleDialogDelegateView> bubble_view_;
  const tab_groups::TabGroupColorId color_id_;
  const std::u16string color_name_;
  bool selected_ = false;
};

BEGIN_METADATA(ColorPickerElementView)
ADD_PROPERTY_METADATA(bool, Selected)
END_METADATA

ColorPickerView::ColorPickerView(
    const views::BubbleDialogDelegateView* bubble_view,
    const TabGroupEditorBubbleView::Colors& colors,
    tab_groups::TabGroupColorId initial_color_id,
    ColorSelectedCallback callback)
    : callback_(std::move(callback)) {
  DCHECK(!colors.empty());

  elements_.reserve(colors.size());
  for (const auto& color : colors) {
    // Create the views for each color, passing them our callback and saving
    // references to them. base::Unretained() is safe here since we delete these
    // views in our destructor, ensuring we outlive them.
    elements_.push_back(AddChildView(std::make_unique<ColorPickerElementView>(
        base::BindRepeating(&ColorPickerView::OnColorSelected,
                            base::Unretained(this)),
        bubble_view, color.first, color.second)));
    if (initial_color_id == color.first) {
      elements_.back()->SetSelected(true);
    }
  }

  // Set the internal padding to be equal to the horizontal insets of a color
  // picker element, since that is the amount by which the color picker's
  // margins should be adjusted to make it visually align with other controls.
  gfx::Insets child_insets = elements_[0]->GetInsets();
  SetProperty(
      views::kInternalPaddingKey,
      gfx::Insets::TLBR(0, child_insets.left(), 0, child_insets.right()));

  // Our children should take keyboard focus, not us.
  SetFocusBehavior(views::View::FocusBehavior::NEVER);
  for (View* view : elements_) {
    // Group the colors so they can be navigated with arrows.
    view->SetGroup(0);
  }

  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kUnbounded)
              .WithAlignment(views::LayoutAlignment::kCenter)
              .WithWeight(1));
}

ColorPickerView::~ColorPickerView() = default;

std::optional<int> ColorPickerView::GetSelectedElement() const {
  for (size_t i = 0; i < elements_.size(); ++i) {
    if (elements_[i]->GetSelected()) {
      return static_cast<int>(i);
    }
  }
  return std::nullopt;
}

views::View* ColorPickerView::GetSelectedViewForGroup(int group) {
  for (ColorPickerElementView* element : elements_) {
    if (element->GetSelected()) {
      return element;
    }
  }
  return nullptr;
}

views::Button* ColorPickerView::GetElementAtIndexForTesting(int index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, static_cast<int>(elements_.size()));
  return elements_[index];
}

void ColorPickerView::OnColorSelected(ColorPickerElementView* element) {
  // Unselect all other elements so that only one can be selected at a time.
  for (ColorPickerElementView* other_element : elements_) {
    if (other_element != element) {
      other_element->SetSelected(false);
    }
  }

  if (callback_) {
    callback_.Run();
  }
}

BEGIN_METADATA(ColorPickerView)
ADD_READONLY_PROPERTY_METADATA(std::optional<int>, SelectedElement)
END_METADATA
