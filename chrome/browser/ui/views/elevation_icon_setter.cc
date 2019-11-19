// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/elevation_icon_setter.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "ui/views/controls/button/label_button.h"

#if defined(OS_WIN)
#include <windows.h>
#include <shellapi.h>

#include "base/task_runner_util.h"
#include "base/win/win_util.h"
#include "ui/display/win/dpi.h"
#include "ui/gfx/icon_util.h"
#endif


// Helpers --------------------------------------------------------------------

namespace {

#if defined(OS_WIN)
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

ElevationIconSetter::ElevationIconSetter(views::LabelButton* button,
                                         base::OnceClosure callback)
    : button_(button) {
#if defined(OS_WIN)
  base::PostTaskAndReplyWithResult(
      base::CreateCOMSTATaskRunner({base::ThreadPool(), base::MayBlock(),
                                    base::TaskPriority::USER_BLOCKING})
          .get(),
      FROM_HERE, base::BindOnce(&GetElevationIcon),
      base::BindOnce(&ElevationIconSetter::SetButtonIcon,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
#endif
}

ElevationIconSetter::~ElevationIconSetter() {
}

void ElevationIconSetter::SetButtonIcon(base::OnceClosure callback,
                                        const SkBitmap& icon) {
  if (!icon.isNull()) {
    float device_scale_factor = 1.0f;
#if defined(OS_WIN)
    // Windows gives us back a correctly-scaled image for the current DPI, so
    // mark this image as having been scaled for the current DPI already.
    device_scale_factor = display::win::GetDPIScale();
#endif
    button_->SetImage(
        views::Button::STATE_NORMAL,
        gfx::ImageSkia(gfx::ImageSkiaRep(icon, device_scale_factor)));
    button_->SizeToPreferredSize();
    if (button_->parent())
      button_->parent()->Layout();
    if (!callback.is_null())
      std::move(callback).Run();
  }
}
