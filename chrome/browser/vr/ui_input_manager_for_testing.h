// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_INPUT_MANAGER_FOR_TESTING_H_
#define CHROME_BROWSER_VR_UI_INPUT_MANAGER_FOR_TESTING_H_

#include "chrome/browser/vr/ui_input_manager.h"

namespace vr {

class UiInputManagerForTesting : public UiInputManager {
 public:
  explicit UiInputManagerForTesting(UiScene* scene);
  bool ControllerRestingInViewport() const override;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_INPUT_MANAGER_FOR_TESTING_H_
