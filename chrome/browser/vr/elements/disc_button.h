// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_DISC_BUTTON_H_
#define CHROME_BROWSER_VR_ELEMENTS_DISC_BUTTON_H_

#include "base/functional/callback.h"
#include "chrome/browser/vr/elements/vector_icon_button.h"
#include "chrome/browser/vr/vr_ui_export.h"
#include "ui/gfx/vector_icon_types.h"

namespace vr {

class VectorIcon;

// A disc button has a circle as the background and a vector icon as the
// foreground. When hovered, background and foreground both move forward on Z
// axis. This matches the Daydream disc-style button.
class VR_UI_EXPORT DiscButton : public VectorIconButton {
 public:
  DiscButton(base::RepeatingCallback<void()> click_handler,
             const gfx::VectorIcon& icon,
             AudioDelegate* audio_delegate);

  DiscButton(const DiscButton&) = delete;
  DiscButton& operator=(const DiscButton&) = delete;

  ~DiscButton() override;

 private:
  void OnSetCornerRadii(const CornerRadii& radii) override;
  void OnSizeAnimated(const gfx::SizeF& size,
                      int target_property_id,
                      gfx::KeyframeModel* keyframe_model) override;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_DISC_BUTTON_H_
