// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/model/model.h"

#include "base/containers/adapters.h"
#include "base/notreached.h"

namespace vr {

namespace {

bool IsOpaqueUiMode(UiMode mode) {
  switch (mode) {
    case kModeBrowsing:
    case kModeFullscreen:
    case kModeWebVr:
    case kModeVoiceSearch:
    case kModeEditingOmnibox:
      return true;
    case kModeRepositionWindow:
    case kModeModalPrompt:
    case kModeVoiceSearchListening:
      return false;
  }
  NOTREACHED();
  return true;
}

}  // namespace

Model::Model() = default;
Model::~Model() = default;

const ColorScheme& Model::color_scheme() const {
  ColorScheme::Mode mode = ColorScheme::kModeNormal;
  return ColorScheme::GetColorScheme(mode);
}

void Model::push_mode(UiMode mode) {
  if (!ui_modes.empty() && ui_modes.back() == mode)
    return;
  ui_modes.push_back(mode);
}

void Model::pop_mode() {
  pop_mode(ui_modes.back());
}

void Model::pop_mode(UiMode mode) {
  if (ui_modes.empty() || ui_modes.back() != mode)
    return;
  // We should always have a mode to be in when we're clearing a mode.
  DCHECK_GE(ui_modes.size(), 2u);
  ui_modes.pop_back();
}

void Model::toggle_mode(UiMode mode) {
  if (!ui_modes.empty() && ui_modes.back() == mode) {
    pop_mode(mode);
    return;
  }
  push_mode(mode);
}

UiMode Model::get_mode() const {
  return ui_modes.back();
}

UiMode Model::get_last_opaque_mode() const {
  for (const UiMode& ui_mode : base::Reversed(ui_modes)) {
    if (IsOpaqueUiMode(ui_mode))
      return ui_mode;
  }
  DCHECK(false) << "get_last_opaque_mode should only be called with at least "
                   "one opaque mode.";
  return kModeBrowsing;
}

bool Model::has_mode_in_stack(UiMode mode) const {
  for (auto stacked_mode : ui_modes) {
    if (mode == stacked_mode)
      return true;
  }
  return false;
}

bool Model::web_vr_enabled() const {
  return get_last_opaque_mode() == kModeWebVr;
}

const ControllerModel& Model::primary_controller() const {
  return controllers[0];
}

ControllerModel& Model::mutable_primary_controller() {
  return controllers[0];
}

}  // namespace vr
