// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_icon_view.h"

#include <memory>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/views/tab_icon_view_model.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/paint_throbber.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

// windows.h needs to come first.  The gap above prevents reordering.
#include <shellapi.h>

#include "chrome/browser/win/app_icon.h"
#include "ui/gfx/icon_util.h"
#endif

namespace {

gfx::ImageSkia CreateDefaultFavicon() {
  gfx::ImageSkia icon;
#if BUILDFLAG(IS_WIN)
  // The default window icon is the application icon, not the default favicon.
  HICON app_icon = GetAppIcon();
  icon = gfx::ImageSkia::CreateFromBitmap(
      IconUtil::CreateSkBitmapFromHICON(app_icon, gfx::Size(16, 16)), 1.0f);
  DestroyIcon(app_icon);
#else
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  icon = *rb.GetImageSkiaNamed(IDR_PRODUCT_LOGO_16);
#endif
  return icon;
}

class DefaultFavicon {
 public:
  DefaultFavicon(const DefaultFavicon&) = delete;
  DefaultFavicon& operator=(const DefaultFavicon&) = delete;

  static const DefaultFavicon& GetInstance() {
    static base::NoDestructor<DefaultFavicon> default_favicon;
    return *default_favicon;
  }

  gfx::ImageSkia icon() const { return icon_; }

 private:
  friend class base::NoDestructor<DefaultFavicon>;

  DefaultFavicon() : icon_(CreateDefaultFavicon()) {}

  const gfx::ImageSkia icon_;
};

}  // namespace

TabIconView::TabIconView() {
  // Inheriting from Button causes this View to be focusable, but it is
  // purely decorative and should not be exposed as focusable in accessibility.
  SetFocusBehavior(FocusBehavior::NEVER);
}

TabIconView::~TabIconView() {
}

void TabIconView::SetModel(TabIconViewModel* model) {
  model_ = model;
  Update();
}

void TabIconView::Update() {
  if (!model_ || !model_->ShouldTabIconViewAnimate())
    throbber_start_time_ = base::TimeTicks();

  SchedulePaint();
}

void TabIconView::PaintThrobber(gfx::Canvas* canvas) {
  if (throbber_start_time_ == base::TimeTicks())
    throbber_start_time_ = base::TimeTicks::Now();

  gfx::PaintThrobberSpinning(canvas, GetLocalBounds(),
                             GetColorProvider()->GetColor(ui::kColorThrobber),
                             base::TimeTicks::Now() - throbber_start_time_);
}

void TabIconView::PaintFavicon(gfx::Canvas* canvas,
                               const gfx::ImageSkia& image) {
  // For source images smaller than the favicon square, scale them as if they
  // were padded to fit the favicon square, so we don't blow up tiny favicons
  // into larger or nonproportional results.
  float float_src_w = static_cast<float>(image.width());
  float float_src_h = static_cast<float>(image.height());
  float scalable_w, scalable_h;
  if (image.width() <= gfx::kFaviconSize &&
      image.height() <= gfx::kFaviconSize) {
    scalable_w = scalable_h = gfx::kFaviconSize;
  } else {
    scalable_w = float_src_w;
    scalable_h = float_src_h;
  }

  // Scale proportionately.
  float scale = std::min(static_cast<float>(width()) / scalable_w,
                         static_cast<float>(height()) / scalable_h);
  int dest_w = static_cast<int>(float_src_w * scale);
  int dest_h = static_cast<int>(float_src_h * scale);

  // Center the scaled image.
  canvas->DrawImageInt(image, 0, 0, image.width(), image.height(),
                       (width() - dest_w) / 2, (height() - dest_h) / 2, dest_w,
                       dest_h, true);
}

gfx::Size TabIconView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(gfx::kFaviconSize, gfx::kFaviconSize);
}

void TabIconView::PaintButtonContents(gfx::Canvas* canvas) {
  if (model_) {
    if (model_->ShouldTabIconViewAnimate()) {
      PaintThrobber(canvas);
      return;
    }

    gfx::ImageSkia favicon =
        model_->GetFaviconForTabIconView().Rasterize(GetColorProvider());
    if (!favicon.isNull()) {
      PaintFavicon(canvas, favicon);
      return;
    }
  }

  PaintFavicon(canvas, DefaultFavicon::GetInstance().icon());
}

BEGIN_METADATA(TabIconView)
END_METADATA
