// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_UI_DISPLAY_SETTINGS_MANAGER_H_
#define CHROMECAST_UI_DISPLAY_SETTINGS_MANAGER_H_

#include <vector>

#include "chromecast/ui/mojom/display_settings.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"

namespace chromecast {

class DisplaySettingsManager {
 public:
  struct ColorTemperatureConfig {
    ColorTemperatureConfig();
    ColorTemperatureConfig(const ColorTemperatureConfig& other);
    ~ColorTemperatureConfig();

    float neutral_temperature;
    float full_color;
    std::vector<float> temperature_values;
    std::vector<float> red_values;
    std::vector<float> green_values;
    std::vector<float> blue_values;
  };

  class Delegate {
   public:
    virtual void SetDisplayBrightness(float brightness, bool smooth) = 0;
    virtual float GetDisplayBrightness() = 0;

   protected:
    virtual ~Delegate() {}
  };

  virtual ~DisplaySettingsManager() = default;

  virtual void SetDelegate(Delegate* delegate) = 0;

  virtual void ResetDelegate() = 0;

  virtual void SetGammaCalibration(
      const std::vector<display::GammaRampRGBEntry>& gamma) = 0;

  virtual void NotifyBrightnessChanged(float new_brightness,
                                       float old_brightness) = 0;

  virtual void SetColorInversion(bool enable) = 0;

  virtual void AddReceiver(
      mojo::PendingReceiver<mojom::DisplaySettings> receiver) = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_UI_DISPLAY_SETTINGS_MANAGER_H_
