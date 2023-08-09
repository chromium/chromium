// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_MODEL_H_
#define CHROME_BROWSER_VR_MODEL_MODEL_H_

#include <memory>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/vr/gl_texture_location.h"
#include "chrome/browser/vr/model/capturing_state_model.h"
#include "chrome/browser/vr/model/color_scheme.h"
#include "chrome/browser/vr/model/controller_model.h"
#include "chrome/browser/vr/model/hosted_platform_ui.h"
#include "chrome/browser/vr/model/platform_toast.h"
#include "chrome/browser/vr/model/reticle_model.h"
#include "chrome/browser/vr/model/ui_mode.h"
#include "chrome/browser/vr/model/web_vr_model.h"
#include "chrome/browser/vr/vr_ui_export.h"
#include "ui/gfx/geometry/transform.h"

namespace vr {

struct VR_UI_EXPORT Model {
  Model();
  ~Model();

  // VR browsing state.
  const ColorScheme& color_scheme() const;
  gfx::Transform projection_matrix;
  bool waiting_for_background = false;
  bool menu_button_long_pressed = false;
  float floor_height = 0.0f;
  bool gvr_input_support = false;
  base::TimeTicks current_time;
  // WebVR state.
  WebVrModel web_vr;

  std::vector<UiMode> ui_modes;
  void push_mode(UiMode mode);
  void pop_mode();
  void pop_mode(UiMode mode);
  void toggle_mode(UiMode mode);
  UiMode get_mode() const;
  UiMode get_last_opaque_mode() const;
  bool has_mode_in_stack(UiMode mode) const;
  bool web_vr_enabled() const;

  // Controller state.
  const ControllerModel& primary_controller() const;
  ControllerModel& mutable_primary_controller();  // For tests
  std::vector<ControllerModel> controllers;
  ReticleModel reticle;

  // State affecting both VR browsing and WebVR.
  CapturingStateModel active_capturing;
  CapturingStateModel background_capturing;
  CapturingStateModel potential_capturing;
  bool skips_redraw_when_not_dirty = false;
  HostedPlatformUi hosted_platform_ui;

  std::unique_ptr<const PlatformToast> platform_toast;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_MODEL_H_
