// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/flying_indicator.h"

#include "base/memory/ptr_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/geometry/cubic_bezier.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/window/dialog_delegate.h"

namespace {
static constexpr base::TimeDelta kFadeInDuration = base::Milliseconds(100);
static constexpr base::TimeDelta kFlyDuration = base::Milliseconds(580);
static constexpr base::TimeDelta kFadeOutDuration = base::Milliseconds(100);
}  // namespace

// static
std::unique_ptr<FlyingIndicator> FlyingIndicator::StartFlyingIndicator(
    const gfx::VectorIcon& icon,
    const gfx::Point& start,
    views::View* target,
    base::OnceClosure done_callback) {
  return base::WrapUnique(
      new FlyingIndicator(icon, start, target, std::move(done_callback)));
}

FlyingIndicator::FlyingIndicator(const gfx::VectorIcon& icon,
                                 const gfx::Point& start,
                                 views::View* target,
                                 base::OnceClosure done_callback)
    : start_(start),
      target_(target),
      animation_(std::vector<gfx::MultiAnimation::Part>{
          gfx::MultiAnimation::Part(kFadeInDuration, gfx::Tween::Type::LINEAR),
          gfx::MultiAnimation::Part(kFlyDuration, gfx::Tween::Type::LINEAR),
          gfx::MultiAnimation::Part(kFadeOutDuration,
                                    gfx::Tween::Type::LINEAR)}),
      done_callback_(std::move(done_callback)) {
  animation_.set_delegate(this);
  animation_.set_continuous(false);

  std::unique_ptr<views::BubbleDialogDelegateView> bubble_view =
      std::make_unique<views::BubbleDialogDelegateView>(
          target, views::BubbleBorder::Arrow::FLOAT,
          views::BubbleBorder::Shadow::STANDARD_SHADOW);

  const auto* color_provider = target_->GetColorProvider();
  const SkColor background_color =
      color_provider->GetColor(kColorFlyingIndicatorBackground);

  // Set the bubble properties.
  bubble_view->SetAccessibleWindowRole(ax::mojom::Role::kNone);
  bubble_view->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  bubble_view->set_margins(gfx::Insets());
  bubble_view->SetCanActivate(false);
  bubble_view->set_focus_traversable_from_anchor_view(false);
  bubble_view->SetBackground(views::CreateSolidBackground(background_color));

  // These constants are from the UX spec.
  static constexpr int kIconSize = 28;
  static constexpr int kBubbleSize = 56;
  static constexpr int kBubbleCornerRadius = 16;

  // Add the link icon.
  auto* const link_image =
      bubble_view->AddChildView(std::make_unique<views::ImageView>());
  link_image->SetImage(ui::ImageModel::FromVectorIcon(
      kWebIcon, kColorFlyingIndicatorForeground, kIconSize));
  link_image->SetPreferredSize(gfx::Size(kBubbleSize, kBubbleSize));

  // Use the default fill layout because there's only one child view.
  bubble_view->SetUseDefaultFillLayout(true);

  // Create the bubble.
  views::BubbleDialogDelegateView* const bubble_view_ptr = bubble_view.get();
  widget_ =
      views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
  scoped_observation_.Observe(widget_.get());

  // Set required frame properties.
  views::BubbleFrameView* const frame_view =
      bubble_view_ptr->GetBubbleFrameView();
  frame_view->set_hit_test_transparent(true);
  frame_view->SetCornerRadius(kBubbleCornerRadius);
  widget_->SetZOrderLevel(ui::ZOrderLevel::kFloatingUIElement);

  // Set up the initial position and opacity, store the desired size, and start
  // the animation.
  widget_->SetOpacity(0.0f);
  widget_->Show();
  bubble_size_ = widget_->GetWindowBoundsInScreen().size();
  animation_.Start();
}

FlyingIndicator::~FlyingIndicator() {
  // Kill the callback before deleting the widget so we don't call it.
  done_callback_.Reset();
  scoped_observation_.Reset();
  if (widget_)
    widget_->Close();
}

void FlyingIndicator::OnWidgetDestroyed(views::Widget* widget) {
  if (widget != widget_)
    return;
  DCHECK(scoped_observation_.IsObserving());
  scoped_observation_.Reset();
  widget_ = nullptr;
  animation_.Stop();
  if (done_callback_)
    std::move(done_callback_).Run();
}

void FlyingIndicator::AnimationProgressed(const gfx::Animation* animation) {
  if (!widget_)
    return;
  if (animation_.current_part_index() > 1U && done_callback_)
    std::move(done_callback_).Run();

  // The steps of the animation are:
  // 0. Grow and fade the bubble in, centered on the originating point.
  // 1. Move the bubble in an arc from starting point to finishing point (over
  //    the counter button).
  // 2. Shrink and fade the bubble out.
  const gfx::Point end_pos = target_->GetBoundsInScreen().CenterPoint();
  gfx::Point location;
  double opacity = 1.0;
  switch (animation_.current_part_index()) {
    case 0:
      location = start_;
      opacity = animation_.GetCurrentValue();
      widget_->SetOpacity(opacity);
      break;
    case 1: {
      const double current_value = animation_.GetCurrentValue();
      const double x_percent =
          gfx::CubicBezier(0.2, 0.0, 1.0, 0.8).Solve(current_value);
      location.set_x(
          std::lround(start_.x() + x_percent * (end_pos.x() - start_.x())));
      const double y_percent =
          gfx::CubicBezier(0.0, 0.0, 0.4, 1.0).Solve(current_value);
      location.set_y(
          std::lround(start_.y() + y_percent * (end_pos.y() - start_.y())));
      break;
    }
    case 2:
      location = end_pos;
      opacity = animation_.CurrentValueBetween(1.0, 0.0);
      widget_->SetOpacity(opacity);
      break;
    default:
      NOTREACHED();
  }
  gfx::Size bubble_size = bubble_size_;
  if (opacity < 1.0) {
    const gfx::Size original_size = bubble_size;
    bubble_size.set_width(std::lround(original_size.width() * opacity));
    bubble_size.set_height(std::lround(original_size.height() * opacity));
  }
  location.Offset(-bubble_size.width() / 2, -bubble_size.height() / 2);
  widget_->SetBounds(gfx::Rect(location, bubble_size));
}

void FlyingIndicator::AnimationEnded(const gfx::Animation* animation) {
  // We need to close the widget, but that can be an asynchronous event, so call
  // |done_callback_| and remove our listeners and reference to the widget.
  if (done_callback_)
    std::move(done_callback_).Run();
  if (widget_) {
    DCHECK(scoped_observation_.IsObserving());
    scoped_observation_.Reset();
    widget_->Close();
    widget_ = nullptr;
  }
}
