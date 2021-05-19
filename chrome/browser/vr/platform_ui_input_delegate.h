// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_PLATFORM_UI_INPUT_DELEGATE_H_
#define CHROME_BROWSER_VR_PLATFORM_UI_INPUT_DELEGATE_H_

#include <memory>
#include <queue>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/vr/input_event.h"
#include "chrome/browser/vr/macros.h"
#include "chrome/browser/vr/model/text_input_info.h"
#include "chrome/browser/vr/text_edit_action.h"
#include "chrome/browser/vr/vr_base_export.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class TimeTicks;
}  // namespace base

namespace gfx {
class PointF;
}  // namespace gfx

namespace vr {

class PlatformInputHandler;

// This class is responsible for processing all events and gestures for
// PlatformUiElement.
class VR_BASE_EXPORT PlatformUiInputDelegate {
 public:
  PlatformUiInputDelegate();
  explicit PlatformUiInputDelegate(PlatformInputHandler* input_handler);
  virtual ~PlatformUiInputDelegate();

  const gfx::Size& size() const { return size_; }

  // The following functions are virtual so that they may be overridden in the
  // MockContentInputDelegate.
  VIRTUAL_FOR_MOCKS void OnHoverEnter(const gfx::PointF& normalized_hit_point,
                                      base::TimeTicks timestamp);
  VIRTUAL_FOR_MOCKS void OnHoverLeave(base::TimeTicks timestamp);
  VIRTUAL_FOR_MOCKS void OnHoverMove(const gfx::PointF& normalized_hit_point,
                                     base::TimeTicks timestamp);
  VIRTUAL_FOR_MOCKS void OnButtonDown(const gfx::PointF& normalized_hit_point,
                                      base::TimeTicks timestamp);
  VIRTUAL_FOR_MOCKS void OnButtonUp(const gfx::PointF& normalized_hit_point,
                                    base::TimeTicks timestamp);
  VIRTUAL_FOR_MOCKS void OnTouchMove(const gfx::PointF& normalized_hit_point,
                                     base::TimeTicks timestamp);
  VIRTUAL_FOR_MOCKS void OnInputEvent(std::unique_ptr<InputEvent> event,
                                      const gfx::PointF& normalized_hit_point);

  void SetSize(int width, int height) { size_ = {width, height}; }
  void SetPlatformInputHandlerForTest(PlatformInputHandler* input_handler) {
    input_handler_ = input_handler;
  }

 protected:
  virtual void SendGestureToTarget(std::unique_ptr<InputEvent> event);
  PlatformInputHandler* input_handler() const { return input_handler_; }

 private:
  void UpdateGesture(const gfx::PointF& normalized_content_hit_point,
                     InputEvent* gesture);
  std::unique_ptr<InputEvent> MakeInputEvent(
      InputEvent::Type type,
      const gfx::PointF& normalized_web_content_location,
      base::TimeTicks time_stamp) const;
  gfx::Point CalculateLocation(
      const gfx::PointF& normalized_web_content_location) const;

  gfx::Size size_;

  PlatformInputHandler* input_handler_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PlatformUiInputDelegate);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_PLATFORM_UI_INPUT_DELEGATE_H_
