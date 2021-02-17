// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_KEYBOARD_DELEGATE_FOR_TESTING_H_
#define CHROME_BROWSER_VR_KEYBOARD_DELEGATE_FOR_TESTING_H_

#include <queue>

#include "base/macros.h"
#include "chrome/browser/vr/keyboard_delegate.h"
#include "chrome/browser/vr/model/text_input_info.h"
#include "chrome/browser/vr/ui_test_input.h"
#include "chrome/browser/vr/vr_base_export.h"

namespace gfx {
class Point3F;
class Transform;
}  // namespace gfx

namespace vr {

class KeyboardUiInterface;
struct CameraModel;

class VR_BASE_EXPORT KeyboardDelegateForTesting : public KeyboardDelegate {
 public:
  KeyboardDelegateForTesting();
  ~KeyboardDelegateForTesting() override;

  void QueueKeyboardInputForTesting(KeyboardTestInput keyboard_input);
  bool IsQueueEmpty() const;

  // KeyboardDelegate implementation.
  void SetUiInterface(KeyboardUiInterface* ui) override;
  void ShowKeyboard() override;
  void HideKeyboard() override;
  void SetTransform(const gfx::Transform&) override;
  bool HitTest(const gfx::Point3F& ray_origin,
               const gfx::Point3F& ray_target,
               gfx::Point3F* touch_position) override;
  void OnBeginFrame() override;
  void Draw(const CameraModel&) override;
  bool SupportsSelection() override;
  void UpdateInput(const TextInputInfo& info) override;

 private:
  KeyboardUiInterface* ui_;
  std::queue<KeyboardTestInput> keyboard_input_queue_;
  TextInputInfo cached_keyboard_input_;
  bool keyboard_shown_ = false;
  bool pause_keyboard_input_ = false;

  DISALLOW_COPY_AND_ASSIGN(KeyboardDelegateForTesting);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_KEYBOARD_DELEGATE_FOR_TESTING_H_
