// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_display_configurator.h"

#include <math.h>
#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "chromecast/base/cast_features.h"
#include "chromecast/browser/cast_touch_device_manager.h"
#include "chromecast/chromecast_buildflags.h"
#include "chromecast/graphics/cast_display_util.h"
#include "chromecast/graphics/cast_screen.h"
#include "chromecast/public/graphics_properties_shlib.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/public/ozone_platform.h"

namespace chromecast {
namespace shell {

namespace {
constexpr int64_t kStubDisplayId = 1;
constexpr char kCastGraphicsHeight[] = "cast-graphics-height";
constexpr char kCastGraphicsWidth[] = "cast-graphics-width";
constexpr char kDisplayRotation[] = "display-rotation";

gfx::Size GetDefaultScreenResolution() {
#if BUILDFLAG(IS_CAST_AUDIO_ONLY)
  return gfx::Size(1, 1);
#else
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!chromecast::IsFeatureEnabled(kTripleBuffer720) &&
      GraphicsPropertiesShlib::IsSupported(GraphicsPropertiesShlib::k1080p,
                                           cmd_line->argv())) {
    return gfx::Size(1920, 1080);
  }

  return gfx::Size(1280, 720);
#endif
}

// Helper to return the screen resolution (device pixels)
// to use.
gfx::Size GetScreenResolution() {
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  int cast_gfx_width = 0;
  int cast_gfx_height = 0;
  if (base::StringToInt(cmd_line->GetSwitchValueASCII(kCastGraphicsWidth),
                        &cast_gfx_width) &&
      base::StringToInt(cmd_line->GetSwitchValueASCII(kCastGraphicsHeight),
                        &cast_gfx_height) &&
      cast_gfx_width > 0 && cast_gfx_height > 0) {
    return gfx::Size(cast_gfx_width, cast_gfx_height);
  }

  return GetDefaultScreenResolution();
}

display::Display::Rotation GetRotationFromCommandLine() {
  std::string rotation =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          kDisplayRotation);
  if (rotation == "90")
    return display::Display::ROTATE_90;
  else if (rotation == "180")
    return display::Display::ROTATE_180;
  else if (rotation == "270")
    return display::Display::ROTATE_270;
  else
    return display::Display::ROTATE_0;
}

gfx::Rect GetScreenBounds(const gfx::Size& size_in_pixels,
                          display::Display::Rotation rotation) {
  switch (rotation) {
    case display::Display::ROTATE_90:
    case display::Display::ROTATE_270:
      return gfx::Rect(
          gfx::Size(size_in_pixels.height(), size_in_pixels.width()));
    case display::Display::ROTATE_0:
    case display::Display::ROTATE_180:
    default:
      return gfx::Rect(size_in_pixels);
  }
}

}  // namespace

CastDisplayConfigurator::CastDisplayConfigurator(CastScreen* screen)
    : delegate_(
#if defined(USE_OZONE) && !BUILDFLAG(IS_CAST_AUDIO_ONLY)
          ui::OzonePlatform::GetInstance()->CreateNativeDisplayDelegate()
#else
          nullptr
#endif
              ),
      touch_device_manager_(std::make_unique<CastTouchDeviceManager>()),
      display_(nullptr),
      cast_screen_(screen),
      weak_factory_(this) {
  if (delegate_) {
    delegate_->AddObserver(this);
    delegate_->Initialize();
    ForceInitialConfigure();
  } else {
    ConfigureDisplayFromCommandLine();
  }
}

CastDisplayConfigurator::~CastDisplayConfigurator() {
  if (delegate_)
    delegate_->RemoveObserver(this);
}

// display::NativeDisplayObserver interface
void CastDisplayConfigurator::OnConfigurationChanged() {
  DCHECK(delegate_);
  delegate_->GetDisplays(base::Bind(
      &CastDisplayConfigurator::OnDisplaysAcquired, weak_factory_.GetWeakPtr(),
      false /* force_initial_configure */));
}

void CastDisplayConfigurator::ConfigureDisplayFromCommandLine() {
  const gfx::Size size = GetScreenResolution();
  UpdateScreen(kStubDisplayId, gfx::Rect(size), GetDeviceScaleFactor(size),
               GetRotationFromCommandLine());
}

void CastDisplayConfigurator::SetColorMatrix(
    const std::vector<float>& color_matrix) {
  if (!delegate_ || !display_)
    return;
  delegate_->SetColorMatrix(display_->display_id(), color_matrix);
}

void CastDisplayConfigurator::SetGammaCorrection(
    const std::vector<display::GammaRampRGBEntry>& degamma_lut,
    const std::vector<display::GammaRampRGBEntry>& gamma_lut) {
  if (!delegate_ || !display_)
    return;

  delegate_->SetGammaCorrection(display_->display_id(), degamma_lut, gamma_lut);
}

void CastDisplayConfigurator::ForceInitialConfigure() {
  if (!delegate_)
    return;
  delegate_->GetDisplays(base::Bind(
      &CastDisplayConfigurator::OnDisplaysAcquired, weak_factory_.GetWeakPtr(),
      true /* force_initial_configure */));
}

void CastDisplayConfigurator::OnDisplaysAcquired(
    bool force_initial_configure,
    const std::vector<display::DisplaySnapshot*>& displays) {
  DCHECK(delegate_);
  if (displays.empty()) {
    LOG(WARNING) << "No displays detected, skipping display init.";
    return;
  }

  if (displays.size() > 1) {
    LOG(WARNING) << "Multiple display detected, using the first one.";
  }

  display_ = displays[0];
  if (!display_->native_mode()) {
    LOG(WARNING) << "Display " << display_->display_id()
                 << " doesn't have a native mode.";
    return;
  }

  gfx::Point origin;
  gfx::Size native_size(display_->native_mode()->size());
  if (force_initial_configure) {
    // For initial configuration, pass the native geometry to gfx::Screen
    // before calling Configure(), so that this information is available
    // to chrome during startup. Otherwise we will not have a valid display
    // during the first queries to display::Screen.
    UpdateScreen(display_->display_id(), gfx::Rect(origin, native_size),
                 GetDeviceScaleFactor(native_size),
                 GetRotationFromCommandLine());
  }

  delegate_->Configure(
      *display_, display_->native_mode(), origin,
      base::BindRepeating(&CastDisplayConfigurator::OnDisplayConfigured,
                          weak_factory_.GetWeakPtr(), display_,
                          display_->native_mode(), origin));
}

void CastDisplayConfigurator::OnDisplayConfigured(
    display::DisplaySnapshot* display,
    const display::DisplayMode* mode,
    const gfx::Point& origin,
    bool success) {
  DCHECK(display);
  DCHECK(mode);
  DCHECK_EQ(display, display_);

  const gfx::Rect bounds(origin, mode->size());
  DVLOG(1) << __func__ << " success=" << success
           << " bounds=" << bounds.ToString();
  if (success) {
    // Need to update the display state otherwise it becomes stale.
    display_->set_current_mode(mode);
    display_->set_origin(origin);

    UpdateScreen(display_->display_id(), bounds,
                 GetDeviceScaleFactor(display->native_mode()->size()),
                 GetRotationFromCommandLine());
  } else {
    LOG(FATAL) << "Failed to configure display";
  }
}

void CastDisplayConfigurator::UpdateScreen(
    int64_t display_id,
    const gfx::Rect& bounds,
    float device_scale_factor,
    display::Display::Rotation rotation) {
  cast_screen_->OnDisplayChanged(display_id, device_scale_factor, rotation,
                                 GetScreenBounds(bounds.size(), rotation));
  touch_device_manager_->OnDisplayConfigured(display_id, rotation, bounds);
}

}  // namespace shell
}  // namespace chromecast
