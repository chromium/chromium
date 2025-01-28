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
#include "ui/display/win/dpi.h"
#include "ui/gfx/win/get_elevation_icon.h"
#endif

// ElevationIconSetter --------------------------------------------------------

ElevationIconSetter::ElevationIconSetter(views::LabelButton* button)
    : button_(button) {
#if BUILDFLAG(IS_WIN)
  base::ThreadPool::CreateCOMSTATaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING})
      ->PostTaskAndReplyWithResult(
          FROM_HERE, base::BindOnce(&gfx::win::GetElevationIcon),
          base::BindOnce(&ElevationIconSetter::SetButtonIcon,
                         weak_factory_.GetWeakPtr()));
#endif
}

ElevationIconSetter::~ElevationIconSetter() = default;

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
