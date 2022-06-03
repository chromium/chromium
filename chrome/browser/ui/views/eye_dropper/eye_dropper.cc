// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/eye_dropper/eye_dropper.h"

#include "build/build_config.h"
#include "content/public/browser/eye_dropper.h"

#if !defined(USE_AURA) && !defined(OS_MAC)
// Used for the platforms that don't support an eye dropper.
std::unique_ptr<content::EyeDropper> ShowEyeDropper(
    content::RenderFrameHost* frame,
    content::EyeDropperListener* listener) {
  return nullptr;
}
#endif  // !defined(USE_AURA) && !defined(OS_MAC)
