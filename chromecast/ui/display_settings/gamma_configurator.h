// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_UI_DISPLAY_SETTINGS_GAMMA_CONFIGURATOR_H_
#define CHROMECAST_UI_DISPLAY_SETTINGS_GAMMA_CONFIGURATOR_H_

#include <vector>

#include "ui/display/types/display_color_management.h"

namespace chromecast {

namespace shell {
class CastDisplayConfigurator;
}  // namespace shell

class GammaConfigurator {
 public:
  explicit GammaConfigurator(
      shell::CastDisplayConfigurator* display_configurator);
  GammaConfigurator(const GammaConfigurator&) = delete;
  GammaConfigurator& operator=(const GammaConfigurator&) = delete;
  ~GammaConfigurator();

  void OnCalibratedGammaLoaded(
      const std::vector<display::GammaRampRGBEntry>& gamma);

  void SetColorInversion(bool enable);

 private:
  void ApplyGammaLut();

  shell::CastDisplayConfigurator* display_configurator_;

  bool is_initialized_ = false;
  bool is_inverted_ = false;
  std::vector<display::GammaRampRGBEntry> gamma_lut_;
};

}  // namespace chromecast

#endif  // CHROMECAST_UI_DISPLAY_SETTINGS_GAMMA_CONFIGURATOR_H_
