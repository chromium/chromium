// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_HOSTED_PLATFORM_UI_H_
#define CHROME_BROWSER_VR_MODEL_HOSTED_PLATFORM_UI_H_

#include "chrome/browser/vr/platform_ui_input_delegate.h"
#include "chrome/browser/vr/vr_base_export.h"
#include "ui/gfx/geometry/rect_f.h"

namespace vr {
typedef PlatformUiInputDelegate* PlatformUiInputDelegatePtr;
struct VR_BASE_EXPORT HostedPlatformUi {
  bool hosted_ui_enabled = false;
  PlatformUiInputDelegatePtr delegate = nullptr;
  unsigned int texture_id = 0;
  bool floating = false;

  // Rectangle's x and y indicate the location of the hosted UI as a percentage
  // of the main content quad size.
  // Rectangle's width and height indicate the size of the hosted UI as a
  // percentage of the main content quad width. If the content quad is not
  // present then width and height are normalized numbers in range [0, 1].
  gfx::RectF rect;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_HOSTED_PLATFORM_UI_H_
