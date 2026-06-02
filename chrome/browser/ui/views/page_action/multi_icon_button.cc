// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/multi_icon_button.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/i18n/number_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkClipOp.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace page_actions {

namespace {

constexpr int kAnchoredMessageIconSize = 20;
constexpr size_t kAnchoredMessageMaxExpandButtonIcons = 3;

// An ImageView that clips its content to a rounded rectangle and also clips out
// the area of a specified "clipper" view to create a cutout effect for
// overlapping icons. The alternative is putting colored rects behind each icon,
// but then the button's background won't show through properly on hover
// effects, etc.
class ClippingImageView : public views::ImageView {
  METADATA_HEADER(ClippingImageView, views::ImageView)
 public:
  ClippingImageView() = default;

  // Sets the view that should be clipped out from this view's canvas.
  void SetClipperView(ClippingImageView* view) { clipper_view_ = view; }

  // views::ImageView:
  void OnPaint(gfx::Canvas* canvas) override {
    canvas->Save();

    constexpr int kIconCornerRadius = 2;
    constexpr int kBorder = 2;
    constexpr int kBorderRadius = kIconCornerRadius + kBorder;

    // Clip this image into a rounded rect.
    SkPathBuilder self_path_builder;
    self_path_builder.addRRect(
        SkRRect::MakeRectXY(gfx::RectToSkRect(GetLocalBounds()),
                            kIconCornerRadius, kIconCornerRadius));
    canvas->ClipPath(self_path_builder.detach(), true);

    // Clip out any other regions that should occlude this image. In its initial
    // use-case, that means the border around the icon stacked directly on top
    // of this one.
    if (clipper_view_) {
      gfx::Rect clipper_bounds = clipper_view_->bounds();
      gfx::Point relative_origin = clipper_bounds.origin();
      views::View::ConvertPointToTarget(clipper_view_->parent(), this,
                                        &relative_origin);
      clipper_bounds.set_origin(relative_origin);

      // Account for the other icon's border.
      clipper_bounds.Inset(-kBorder);

      SkRRect rrect;
      rrect.setRectXY(gfx::RectToSkRect(clipper_bounds), kBorderRadius,
                      kBorderRadius);

      // Apply difference clip to cut out the overlapping area.
      canvas->sk_canvas()->clipRRect(rrect, SkClipOp::kDifference, true);
    }

    views::ImageView::OnPaint(canvas);
    canvas->Restore();
  }

 private:
  // View that is "above" this view in Z-order and should be clipped out.
  // Dangling pointer detection disabled because the Views are destroyed
  // in order, and the clip is irrelevant at that time.
  raw_ptr<ClippingImageView, DisableDanglingPtrDetection> clipper_view_ =
      nullptr;
};

BEGIN_METADATA(ClippingImageView)
END_METADATA

}  // namespace

MultiIconButton::MultiIconButton(PressedCallback callback)
    : views::Button(std::move(callback)) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
                       views::BoxLayout::Orientation::kHorizontal,
                       gfx::Insets::VH(6, 8), -6))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  SetBorder(nullptr);
  SetInstallFocusRingOnFocus(true);
  SetAccessibleName(u"Show details");
}

MultiIconButton::~MultiIconButton() = default;

views::View::Views MultiIconButton::GetChildrenInZOrder() {
  auto children = views::Button::GetChildrenInZOrder();
  std::reverse(children.begin(), children.end());
  return children;
}

void MultiIconButton::OnThemeChanged() {
  views::Button::OnThemeChanged();
  const auto* color_provider = GetColorProvider();
  if (color_provider) {
    SetBackground(views::CreateRoundedRectBackground(
        color_provider->GetColor(ui::kColorSysPrimaryContainer), 12));
    if (plus_more_label_) {
      plus_more_label_->SetEnabledColor(
          color_provider->GetColor(ui::kColorSysOnPrimaryContainer));
    }
  }
}

void MultiIconButton::Update(
    const std::vector<std::reference_wrapper<const ui::ImageModel>>& icons) {
  plus_more_label_ = nullptr;
  RemoveAllChildViews();
  // When adding each icon, clip it using the previous icon.
  ClippingImageView* previous_icon = nullptr;
  const size_t num_icons =
      std::min(icons.size(), kAnchoredMessageMaxExpandButtonIcons);
  for (size_t i = 0; i < num_icons; ++i) {
    const ui::ImageModel& icon = icons[i];
    if (!icon.IsEmpty()) {
      auto* icon_view = AddChildView(std::make_unique<ClippingImageView>());
      icon_view->SetImage(icon);
      icon_view->SetImageSize(
          gfx::Size(kAnchoredMessageIconSize, kAnchoredMessageIconSize));
      if (previous_icon) {
        icon_view->SetClipperView(previous_icon);
      }
      previous_icon = icon_view;
    }
  }

  if (icons.size() > kAnchoredMessageMaxExpandButtonIcons) {
    plus_more_label_ = AddChildView(std::make_unique<views::Label>(base::StrCat(
        {u"+", base::FormatNumber(icons.size() -
                                  kAnchoredMessageMaxExpandButtonIcons)})));
    plus_more_label_->SetTextStyle(views::style::STYLE_BODY_5);
    plus_more_label_->SetProperty(views::kMarginsKey,
                                  gfx::Insets::TLBR(0, 8, 0, 0));
  }
}

BEGIN_METADATA(MultiIconButton)
END_METADATA

}  // namespace page_actions
