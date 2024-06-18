// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fullscreen_control/fullscreen_control_view.h"

#include <memory>

#include "base/functional/callback.h"
#include "cc/paint/paint_flags.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/vector_icons.h"

namespace {

// Partially-transparent background color.
const SkColor kButtonBackgroundColor = SkColorSetARGB(0xcc, 0x28, 0x2c, 0x32);

constexpr int kCloseIconSize = 24;

class CloseFullscreenButton : public views::Button {
  METADATA_HEADER(CloseFullscreenButton, views::Button)

 public:
  explicit CloseFullscreenButton(PressedCallback callback)
      : views::Button(std::move(callback)) {
    std::unique_ptr<views::ImageView> close_image_view =
        std::make_unique<views::ImageView>();
    close_image_view->SetImage(ui::ImageModel::FromVectorIcon(
        views::kIcCloseIcon, SK_ColorWHITE, kCloseIconSize));
    // Not focusable by default, only for accessibility.
    SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
    GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_EXIT_FULLSCREEN_MODE));
    AddChildView(close_image_view.release());
    SetLayoutManager(std::make_unique<views::FillLayout>());
  }
  CloseFullscreenButton(const CloseFullscreenButton&) = delete;
  CloseFullscreenButton& operator=(const CloseFullscreenButton&) = delete;

 private:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    // TODO(robliao): If we decide to move forward with this, use themes.
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(kButtonBackgroundColor);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    float radius = FullscreenControlView::kCircleButtonDiameter / 2.0f;
    canvas->DrawCircle(gfx::PointF(radius, radius), radius, flags);
  }
};

BEGIN_METADATA(CloseFullscreenButton)
END_METADATA

}  // namespace

FullscreenControlView::FullscreenControlView(
    views::Button::PressedCallback callback) {
  exit_fullscreen_button_ = AddChildView(
      std::make_unique<CloseFullscreenButton>(std::move(callback)));
  SetLayoutManager(std::make_unique<views::FillLayout>());
  exit_fullscreen_button_->SetPreferredSize(
      gfx::Size(kCircleButtonDiameter, kCircleButtonDiameter));
}

FullscreenControlView::~FullscreenControlView() = default;

BEGIN_METADATA(FullscreenControlView)
END_METADATA
