// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/ui/display_settings/gamma_configurator.h"

#include <limits>

#include "chromecast/browser/cast_display_configurator.h"

namespace chromecast {
namespace {

constexpr size_t kGammaTableSize = 256;
constexpr uint16_t kMaxGammaValue = std::numeric_limits<uint16_t>::max();

std::vector<display::GammaRampRGBEntry> CreateDefaultGammaLut() {
  std::vector<display::GammaRampRGBEntry> gamma_lut;
  for (size_t i = 0; i < kGammaTableSize; ++i) {
    const uint16_t value = static_cast<uint16_t>(
        kMaxGammaValue *
        (static_cast<float>(kGammaTableSize - i - 1) / (kGammaTableSize - 1)));
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
    shell::CastDisplayConfigurator* display_configurator)
    : display_configurator_(display_configurator) {
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
  display::GammaAdjustment adjustment;
  adjustment.curve = display::GammaCurve(
      is_inverted_ ? InvertGammaLut(gamma_lut_) : gamma_lut_);
  display_configurator_->SetGammaAdjustment(adjustment);
}

void GammaConfigurator::SetColorInversion(bool invert) {
  if (is_inverted_ == invert)
    return;

  is_inverted_ = invert;

  if (is_initialized_)
    ApplyGammaLut();
}

}  // namespace chromecast
