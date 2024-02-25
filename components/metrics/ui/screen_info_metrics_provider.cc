// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/ui/screen_info_metrics_provider.h"

#include <algorithm>

#include "build/build_config.h"
#include "third_party/metrics_proto/system_profile.pb.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace metrics {

#if BUILDFLAG(IS_WIN)

namespace {

struct ScreenDPIInformation {
  double max_dpi_x;
  double max_dpi_y;
};

// Called once for each connected monitor.
BOOL CALLBACK GetMonitorDPICallback(HMONITOR, HDC hdc, LPRECT, LPARAM dwData) {
  const double kMillimetersPerInch = 25.4;
  ScreenDPIInformation* screen_info =
      reinterpret_cast<ScreenDPIInformation*>(dwData);
  // Size of screen, in mm.
  DWORD size_x = GetDeviceCaps(hdc, HORZSIZE);
  DWORD size_y = GetDeviceCaps(hdc, VERTSIZE);
  double dpi_x = (size_x > 0) ?
      GetDeviceCaps(hdc, HORZRES) / (size_x / kMillimetersPerInch) : 0;
  double dpi_y = (size_y > 0) ?
      GetDeviceCaps(hdc, VERTRES) / (size_y / kMillimetersPerInch) : 0;
  screen_info->max_dpi_x = std::max(dpi_x, screen_info->max_dpi_x);
  screen_info->max_dpi_y = std::max(dpi_y, screen_info->max_dpi_y);
  return TRUE;
}

void WriteScreenDPIInformationProto(SystemProfileProto::Hardware* hardware) {
  HDC desktop_dc = GetDC(nullptr);
  if (desktop_dc) {
    ScreenDPIInformation si = {0, 0};
    if (EnumDisplayMonitors(desktop_dc, nullptr, GetMonitorDPICallback,
                            reinterpret_cast<LPARAM>(&si))) {
      hardware->set_max_dpi_x(si.max_dpi_x);
      hardware->set_max_dpi_y(si.max_dpi_y);
    }
    ReleaseDC(GetDesktopWindow(), desktop_dc);
  }
}

}  // namespace

#endif  // BUILDFLAG(IS_WIN)

ScreenInfoMetricsProvider::ScreenInfoMetricsProvider() {
}

ScreenInfoMetricsProvider::~ScreenInfoMetricsProvider() {
}

void ScreenInfoMetricsProvider::ProvideSystemProfileMetrics(
    SystemProfileProto* system_profile_proto) {
  // This may be called before the screen info has been initialized, such as
  // when the persistent system profile gets filled in initially.
  const std::optional<gfx::Size> display_size = GetScreenSize();
  if (!display_size.has_value())
    return;

  SystemProfileProto::Hardware* hardware =
      system_profile_proto->mutable_hardware();

  hardware->set_primary_screen_width(display_size->width());
  hardware->set_primary_screen_height(display_size->height());
  hardware->set_primary_screen_scale_factor(GetScreenDeviceScaleFactor());
  hardware->set_screen_count(GetScreenCount());

#if BUILDFLAG(IS_WIN)
  WriteScreenDPIInformationProto(hardware);
#endif
}

std::optional<gfx::Size> ScreenInfoMetricsProvider::GetScreenSize() const {
  auto* screen = display::Screen::GetScreen();
  if (!screen)
    return std::nullopt;
  return std::make_optional(screen->GetPrimaryDisplay().GetSizeInPixel());
}

float ScreenInfoMetricsProvider::GetScreenDeviceScaleFactor() const {
  return display::Screen::GetScreen()
      ->GetPrimaryDisplay()
      .device_scale_factor();
}

int ScreenInfoMetricsProvider::GetScreenCount() const {
  return display::Screen::GetScreen()->GetNumDisplays();
}

}  // namespace metrics
