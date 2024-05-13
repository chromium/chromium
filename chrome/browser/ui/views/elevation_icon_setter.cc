// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/elevation_icon_setter.h"

#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/views/controls/button/label_button.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <shellapi.h>

#include "base/win/win_util.h"
#include "ui/display/win/dpi.h"
#include "ui/gfx/icon_util.h"
#endif


// Helpers --------------------------------------------------------------------

namespace {

#if BUILDFLAG(IS_WIN)
SkBitmap GetElevationIcon() {
  if (!base::win::UserAccountControlIsEnabled())
    return SkBitmap();

  SHSTOCKICONINFO icon_info = { sizeof(SHSTOCKICONINFO) };
  if (FAILED(SHGetStockIconInfo(SIID_SHIELD, SHGSI_ICON | SHGSI_SMALLICON,
                                &icon_info)))
    return SkBitmap();

  SkBitmap icon = IconUtil::CreateSkBitmapFromHICON(
      icon_info.hIcon,
      gfx::Size(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON)));
  DestroyIcon(icon_info.hIcon);
  return icon;
}
#endif

}  // namespace


// ElevationIconSetter --------------------------------------------------------

ElevationIconSetter::ElevationIconSetter(views::LabelButton* button)
    : button_(button) {
#if BUILDFLAG(IS_WIN)
  base::ThreadPool::CreateCOMSTATaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING})
      ->PostTaskAndReplyWithResult(
          FROM_HERE, base::BindOnce(&GetElevationIcon),
          base::BindOnce(&ElevationIconSetter::SetButtonIcon,
                         weak_factory_.GetWeakPtr()));
#endif
}

ElevationIconSetter::~ElevationIconSetter() {
}

void ElevationIconSetter::SetButtonIcon(const SkBitmap& icon) {
  if (!icon.isNull()) {
    float device_scale_factor = 1.0f;
#if BUILDFLAG(IS_WIN)
    // Windows gives us back a correctly-scaled image for the current DPI, so
    // mark this image as having been scaled for the current DPI already.
    device_scale_factor = display::win::GetDPIScale();
#endif
    button_->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromImageSkia(
            gfx::ImageSkia::CreateFromBitmap(icon, device_scale_factor)));
  }
}
