// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_OVAL_H_
#define CHROME_BROWSER_VR_ELEMENTS_OVAL_H_

#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/vr_ui_export.h"

namespace vr {

// An oval behaves like a rect save for the fact that it manages its own corner
// radii to ensure circular right and left end caps.
class VR_UI_EXPORT Oval : public Rect {
 public:
  Oval();

  Oval(const Oval&) = delete;
  Oval& operator=(const Oval&) = delete;

  ~Oval() override;

 private:
  void OnSizeAnimated(const gfx::SizeF& size,
                      int target_property_id,
                      gfx::KeyframeModel* keyframe_model) override;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_OVAL_H_
