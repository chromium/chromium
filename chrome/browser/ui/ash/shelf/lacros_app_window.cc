// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/lacros_app_window.h"

#include "build/branding_buildflags.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "extensions/common/constants.h"
#include "skia/ext/image_operations.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/widget/native_widget_aura.h"

namespace {
constexpr int kWindowIconSizeDips = extension_misc::EXTENSION_ICON_BITTY;
constexpr int kAppIconSizeDips = extension_misc::EXTENSION_ICON_MEDIUM;
}  // namespace

LacrosAppWindow::LacrosAppWindow(const ash::ShelfID& shelf_id,
                                 views::Widget* widget)
    : AppWindowBase(shelf_id, widget) {
  // The lacros browser icon is known to ash, so don't bother with app service
  // icon loading.
  // TODO(jamescook): Replace the canary icon with the chrome icon. Use an icon
  // that is the correct size for the app icon instead of resizing in software.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const int icon_id = IDR_PRODUCT_LOGO_256_CANARY;
#else
  const int icon_id = IDR_PRODUCT_LOGO_256;
#endif
  gfx::ImageSkia icon =
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(icon_id);
  // The app icon is large.
  gfx::ImageSkia app_icon = gfx::ImageSkiaOperations::CreateResizedImage(
      icon, skia::ImageOperations::RESIZE_BEST,
      gfx::Size(kAppIconSizeDips, kAppIconSizeDips));
  // The window icon is small.
  gfx::ImageSkia window_icon = gfx::ImageSkiaOperations::CreateResizedImage(
      app_icon, skia::ImageOperations::RESIZE_BEST,
      gfx::Size(kWindowIconSizeDips, kWindowIconSizeDips));
  views::NativeWidgetAura::AssignIconToAuraWindow(GetNativeWindow(),
                                                  window_icon, app_icon);
}

LacrosAppWindow::~LacrosAppWindow() = default;
