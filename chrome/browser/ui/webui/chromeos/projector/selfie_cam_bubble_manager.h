// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_PROJECTOR_SELFIE_CAM_BUBBLE_MANAGER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_PROJECTOR_SELFIE_CAM_BUBBLE_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "ui/gfx/geometry/rect.h"

class Profile;
class WebUIBubbleDialogView;

namespace chromeos {

// Handles the creation and destruction of the selfie cam WebUI bubble.
class SelfieCamBubbleManager {
 public:
  SelfieCamBubbleManager();
  SelfieCamBubbleManager(const SelfieCamBubbleManager&) = delete;
  SelfieCamBubbleManager& operator=(const SelfieCamBubbleManager&) = delete;
  ~SelfieCamBubbleManager();

  void Show(Profile* profile, const gfx::Rect& context_bounds_in_screen);
  void Close();
  bool IsVisible() const;

 private:
  base::WeakPtr<WebUIBubbleDialogView> bubble_view_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_PROJECTOR_SELFIE_CAM_BUBBLE_MANAGER_H_
