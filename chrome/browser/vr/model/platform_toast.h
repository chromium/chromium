// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_PLATFORM_TOAST_H_
#define CHROME_BROWSER_VR_MODEL_PLATFORM_TOAST_H_

#include <string>

#include "chrome/browser/vr/vr_ui_export.h"

namespace vr {

// Represent a request to show a text only Toast.
struct VR_UI_EXPORT PlatformToast {
  PlatformToast();
  explicit PlatformToast(std::u16string text);

  std::u16string text;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_PLATFORM_TOAST_H_
