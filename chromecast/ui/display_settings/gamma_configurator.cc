// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/ui/display_settings/gamma_configurator.h"

#include <limits>

#include "chromecast/browser/cast_display_configurator.h"
#include "chromecast/graphics/cast_window_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"

namespace chromecast {
namespace {

constexpr size_t kGammaTableSize = 256;
constexpr uint16_t kMaxGammaValue = std::numeric_limits<uint16_t>::max();

std::vector<display::GammaRampRGBEntry> CreateDefaultGammaLut() {
  std::vector<display::GammaRampRGBEntry> gamma_lut;
  for (size_t i = 0; i < kGammaTableSize; ++i) {
    float value =
        static_cast<float>(kGammaTableSize - i - 1) / (kGammaTableSize - 1);
    value *= kMaxGammaValue;
    gamma_lut.push_back({value, value, value});
  }

  return gamma_lut;
}

std::vector<display::GammaRampRGBEntry> InvertGammaLut(
    const std::vector<display::GammaRampRGBEntry>& gamma_lut) {
  // Use default reversed linear gamma LUT for inversion
  if (gamma_lut.empty())
    return CreateDefaultGammaLut();

  // Use calibrated gamma LUT in reverse for inversion
  std::vector<display::GammaRampRGBEntry> gamma_lut_inverted(gamma_lut.rbegin(),
                                                             gamma_lut.rend());
  return gamma_lut_inverted;
}

}  // namespace

GammaConfigurator::GammaConfigurator(
    CastWindowManager* window_manager,
    shell::CastDisplayConfigurator* display_configurator)
    : window_manager_(window_manager),
      display_configurator_(display_configurator) {
  DCHECK(window_manager_);
  DCHECK(display_configurator_);
}

GammaConfigurator::~GammaConfigurator() = default;

void GammaConfigurator::OnCalibratedGammaLoaded(
    const std::vector<display::GammaRampRGBEntry>& gamma) {
  is_initialized_ = true;
  gamma_lut_ = gamma;

  if (is_inverted_)
    ApplyGammaLut();
}

void GammaConfigurator::ApplyGammaLut() {
  if (is_inverted_)
    display_configurator_->SetGammaCorrection({}, InvertGammaLut(gamma_lut_));
  else
    display_configurator_->SetGammaCorrection({}, gamma_lut_);

  // The LUT is applied on the next swap buffers, so we need to make sure the
  // root window triggers a swap buffer otherwise the content will not update.
  window_manager_->GetRootWindow()
      ->GetHost()
      ->compositor()
      ->ScheduleFullRedraw();
}

void GammaConfigurator::SetColorInversion(bool invert) {
  is_inverted_ = invert;

  if (is_initialized_)
    ApplyGammaLut();
}

}  // namespace chromecast
