// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SCHEDULER_BROWSER_RENDERER_INTERFACE_H_
#define CHROME_BROWSER_VR_SCHEDULER_BROWSER_RENDERER_INTERFACE_H_

namespace base {
class TimeTicks;
}

namespace gfx {
class Transform;
}

namespace vr {

class SchedulerBrowserRendererInterface {
 public:
  virtual ~SchedulerBrowserRendererInterface() {}
  virtual void DrawBrowserFrame(base::TimeTicks current_time) = 0;
  // Pass the same head_pose used to render the submitted WebXR frame.
  virtual void DrawWebXrFrame(base::TimeTicks current_time,
                              const gfx::Transform& head_pose) = 0;
  virtual void ProcessControllerInputForWebXr(const gfx::Transform& head_pose,
                                              base::TimeTicks current_time) = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SCHEDULER_BROWSER_RENDERER_INTERFACE_H_
