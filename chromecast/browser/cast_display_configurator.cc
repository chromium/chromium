// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_display_configurator.h"

#include <math.h>

#include <algorithm>
#include <string>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chromecast/base/cast_features.h"
#include "chromecast/browser/cast_touch_device_manager.h"
#include "chromecast/chromecast_buildflags.h"
#include "chromecast/graphics/cast_display_util.h"
#include "chromecast/graphics/cast_screen.h"
#include "chromecast/public/graphics_properties_shlib.h"
#include "ui/display/types/display_configuration_params.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/public/ozone_platform.h"

namespace chromecast {
namespace shell {

namespace {
constexpr int64_t kStubDisplayId = 1;
constexpr char kCastGraphicsHeight[] = "cast-graphics-height";
constexpr char kCastGraphicsWidth[] = "cast-graphics-width";

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

display::Display::Rotation RotationFromPanelOrientation(
    display::PanelOrientation orientation) {
  switch (orientation) {
    case display::kNormal:
      return display::Display::ROTATE_0;
    case display::kRightUp:
      return display::Display::ROTATE_90;
    case display::kBottomUp:
      return display::Display::ROTATE_180;
    case display::kLeftUp:
      return display::Display::ROTATE_270;
  }
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
#if BUILDFLAG(IS_OZONE) && !BUILDFLAG(IS_CAST_AUDIO_ONLY)
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
  delegate_->GetDisplays(base::BindOnce(
      &CastDisplayConfigurator::OnDisplaysAcquired, weak_factory_.GetWeakPtr(),
      false /* force_initial_configure */));
}

void CastDisplayConfigurator::OnDisplaySnapshotsInvalidated() {
  display_ = nullptr;
}

void CastDisplayConfigurator::EnableDisplay(
    display::ConfigureCallback callback) {
  if (!delegate_ || !display_)
    return;

  display::DisplayConfigurationParams display_config_params(
      display_->display_id(), gfx::Point(), display_->native_mode());
  std::vector<display::DisplayConfigurationParams> config_request;
  config_request.push_back(std::move(display_config_params));

  delegate_->Configure(config_request, std::move(callback),
                       {display::ModesetFlag::kTestModeset,
                        display::ModesetFlag::kCommitModeset});
  NotifyObservers();
}

void CastDisplayConfigurator::DisableDisplay(
    display::ConfigureCallback callback) {
  if (!delegate_ || !display_)
    return;

  display::DisplayConfigurationParams display_config_params(
      display_->display_id(), gfx::Point(), nullptr);
  std::vector<display::DisplayConfigurationParams> config_request;
  config_request.push_back(std::move(display_config_params));

  delegate_->Configure(config_request, std::move(callback),
                       {display::ModesetFlag::kTestModeset,
                        display::ModesetFlag::kCommitModeset});
}

void CastDisplayConfigurator::ConfigureDisplayFromCommandLine() {
  const gfx::Size size = GetScreenResolution();
  UpdateScreen(kStubDisplayId, gfx::Rect(size), GetDeviceScaleFactor(size),
               display::Display::ROTATE_0);
}

void CastDisplayConfigurator::SetColorTemperatureAdjustment(
    const display::ColorTemperatureAdjustment& cta) {
  if (!delegate_ || !display_)
    return;
  delegate_->SetColorTemperatureAdjustment(display_->display_id(), cta);

  NotifyObservers();
}

void CastDisplayConfigurator::SetGammaAdjustment(
    const display::GammaAdjustment& adjustment) {
  if (!delegate_ || !display_)
    return;
  delegate_->SetGammaAdjustment(display_->display_id(), adjustment);
  NotifyObservers();
}

void CastDisplayConfigurator::NotifyObservers() {
  for (Observer& observer : observers_)
    observer.OnDisplayStateChanged();
}

void CastDisplayConfigurator::ForceInitialConfigure() {
  if (!delegate_)
    return;
  delegate_->GetDisplays(base::BindOnce(
      &CastDisplayConfigurator::OnDisplaysAcquired, weak_factory_.GetWeakPtr(),
      true /* force_initial_configure */));
}

void CastDisplayConfigurator::OnDisplaysAcquired(
    bool force_initial_configure,
    const std::vector<raw_ptr<display::DisplaySnapshot, VectorExperimental>>&
        displays) {
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
                 RotationFromPanelOrientation(display_->panel_orientation()));
  }

  display::DisplayConfigurationParams display_config_params(
      display_->display_id(), origin, display_->native_mode());
  std::vector<display::DisplayConfigurationParams> config_request;
  config_request.push_back(std::move(display_config_params));

  delegate_->Configure(
      config_request,
      base::BindRepeating(&CastDisplayConfigurator::OnDisplayConfigured,
                          weak_factory_.GetWeakPtr()),
      {display::ModesetFlag::kTestModeset,
       display::ModesetFlag::kCommitModeset});
}

void CastDisplayConfigurator::OnDisplayConfigured(
    const std::vector<display::DisplayConfigurationParams>& request_results,
    bool config_success) {
  DCHECK_EQ(request_results.size(), 1u);
  const auto& result = request_results[0];
  DCHECK(result.mode);

  // Discard events for previous configurations. It is safe to discard since a
  // new configuration round was initiated and we're waiting for another
  // OnDisplayConfigured() event with the up-to-date display to arrive.
  //
  // This typically only happens when there's crashes and the state updates at
  // the same time old notifications are received.
  if (result.id != display_->display_id()) {
    return;
  }

  const gfx::Rect bounds(result.origin, result.mode->size());
  DVLOG(1) << __func__ << " success=" << config_success
           << " bounds=" << bounds.ToString();
  if (config_success) {
    // Need to update the display state otherwise it becomes stale.
    display_->set_current_mode(result.mode.get());
    display_->set_origin(result.origin);

    UpdateScreen(display_->display_id(), bounds,
                 GetDeviceScaleFactor(display_->native_mode()->size()),
                 RotationFromPanelOrientation(display_->panel_orientation()));
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

void CastDisplayConfigurator::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CastDisplayConfigurator::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace shell
}  // namespace chromecast
