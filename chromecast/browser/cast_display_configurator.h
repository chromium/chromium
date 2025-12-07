// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_DISPLAY_CONFIGURATOR_H_
#define CHROMECAST_BROWSER_CAST_DISPLAY_CONFIGURATOR_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ui/display/display.h"
#include "ui/display/types/display_color_management.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/display/types/native_display_observer.h"

namespace display {
class DisplaySnapshot;

struct DisplayConfigurationParams;
}  // namespace display

namespace chromecast {
class CastScreen;

namespace shell {
class CastTouchDeviceManager;

// The CastDisplayConfigurator class ensures native displays are initialized and
// configured properly on platforms that need that (e.g. GBM/DRM graphics via
// OzonePlatformGbm on odroid). But OzonePlatformCast, used by most Cast
// devices, relies on the platform code (outside of cast_shell) to initialize
// displays and exposes only a FakeDisplayDelegate. So CastDisplayConfigurator
// doesn't really do anything when using OzonePlatformCast.
class CastDisplayConfigurator : public display::NativeDisplayObserver {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnDisplayStateChanged() = 0;
  };

  explicit CastDisplayConfigurator(CastScreen* screen);

  CastDisplayConfigurator(const CastDisplayConfigurator&) = delete;
  CastDisplayConfigurator& operator=(const CastDisplayConfigurator&) = delete;

  ~CastDisplayConfigurator() override;

  // display::NativeDisplayObserver implementation
  void OnConfigurationChanged() override;
  void OnDisplaySnapshotsInvalidated() override;

  void EnableDisplay(display::ConfigureCallback callback);
  void DisableDisplay(display::ConfigureCallback callback);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void ConfigureDisplayFromCommandLine();
  void SetColorTemperatureAdjustment(
      const display::ColorTemperatureAdjustment& cta);
  void SetGammaAdjustment(const display::GammaAdjustment& adjustment);

 private:
  void ForceInitialConfigure();
  void NotifyObservers();
  void OnDisplaysAcquired(
      bool force_initial_configure,
      const std::vector<raw_ptr<display::DisplaySnapshot, VectorExperimental>>&
          displays);
  void OnDisplayConfigured(
      const std::vector<display::DisplayConfigurationParams>& request_results,
      bool statuses);
  void UpdateScreen(int64_t display_id,
                    const gfx::Rect& bounds,
                    float device_scale_factor,
                    display::Display::Rotation rotation);

  base::ObserverList<Observer>::Unchecked observers_;

  std::unique_ptr<display::NativeDisplayDelegate> delegate_;
  std::unique_ptr<CastTouchDeviceManager> touch_device_manager_;
  display::DisplaySnapshot* display_;
  CastScreen* const cast_screen_;

  base::WeakPtrFactory<CastDisplayConfigurator> weak_factory_;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_DISPLAY_CONFIGURATOR_H_
