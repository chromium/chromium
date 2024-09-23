// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_MODEL_H_
#define CHROME_BROWSER_VR_MODEL_MODEL_H_

#include "base/time/time.h"
#include "chrome/browser/vr/model/capturing_state_model.h"
#include "chrome/browser/vr/model/color_scheme.h"
#include "chrome/browser/vr/model/web_vr_model.h"
#include "chrome/browser/vr/vr_ui_export.h"
#include "ui/gfx/geometry/transform.h"

namespace vr {

struct VR_UI_EXPORT Model {
  Model();
  ~Model();

  // VR browsing state.
  const ColorScheme& color_scheme() const;
  // WebVR state.
  WebVrModel web_vr;

  // State affecting both VR browsing and WebVR.
  CapturingStateModel active_capturing;
  CapturingStateModel background_capturing;
  CapturingStateModel potential_capturing;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_MODEL_H_
