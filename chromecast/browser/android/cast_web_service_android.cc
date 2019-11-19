// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_web_service.h"

#include <memory>

#include "chromecast/browser/android/cast_content_window_android.h"

namespace chromecast {

std::unique_ptr<CastContentWindow> CastWebService::CreateWindow(
    const CastContentWindow::CreateParams& params) {
  return std::make_unique<CastContentWindowAndroid>(params);
}

}  // namespace chromecast
