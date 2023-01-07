// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_SPINNER_H_
#define CHROME_BROWSER_VR_ELEMENTS_SPINNER_H_

#include <memory>

#include "chrome/browser/vr/elements/textured_element.h"
#include "chrome/browser/vr/vr_ui_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace vr {

class SpinnerTexture;

class VR_UI_EXPORT Spinner : public TexturedElement {
 public:
  explicit Spinner(int texture_width);

  Spinner(const Spinner&) = delete;
  Spinner& operator=(const Spinner&) = delete;

  ~Spinner() override;

  void SetColor(SkColor color);

 protected:
  UiTexture* GetTexture() const override;

 private:
  bool TextureDependsOnMeasurement() const override;
  gfx::Size MeasureTextureSize() override;

  void OnFloatAnimated(const float& value,
                       int target_property_id,
                       gfx::KeyframeModel* keyframe_model) override;
  std::unique_ptr<SpinnerTexture> texture_;
  int texture_width_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_SPINNER_H_
