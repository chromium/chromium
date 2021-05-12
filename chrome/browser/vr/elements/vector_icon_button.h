// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_VECTOR_ICON_BUTTON_H_
#define CHROME_BROWSER_VR_ELEMENTS_VECTOR_ICON_BUTTON_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/vr/elements/button.h"
#include "chrome/browser/vr/vr_ui_export.h"
#include "ui/gfx/vector_icon_types.h"

namespace vr {

class VectorIcon;

// A vector button has rect as a background and a vector icon as the
// foreground. When hovered, background and foreground both move forward on Z
// axis.
class VR_UI_EXPORT VectorIconButton : public Button {
 public:
  VectorIconButton(base::RepeatingCallback<void()> click_handler,
                   const gfx::VectorIcon& icon,
                   AudioDelegate* audio_delegate);
  ~VectorIconButton() override;

  VectorIcon* foreground() const { return foreground_; }

  void SetIcon(const gfx::VectorIcon& icon);
  void SetIconTranslation(float x, float y);
  void SetIconScaleFactor(float factor);
  float icon_scale_factor() const { return icon_scale_factor_; }

 private:
  void OnStateUpdated() override;
  void OnSetDrawPhase() override;
  void OnSetName() override;
  void OnSetSize(const gfx::SizeF& size) override;

  // This button will automatically scale down the given icon to fit the button.
  // This value is used to determine the amount of scaling and can be set
  // externally to create a smaller or larger icon.
  float icon_scale_factor_;
  VectorIcon* foreground_;
  DISALLOW_COPY_AND_ASSIGN(VectorIconButton);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_VECTOR_ICON_BUTTON_H_
