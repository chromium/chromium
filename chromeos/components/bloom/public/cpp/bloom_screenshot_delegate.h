// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_BLOOM_PUBLIC_CPP_BLOOM_SCREENSHOT_DELEGATE_H_
#define CHROMEOS_COMPONENTS_BLOOM_PUBLIC_CPP_BLOOM_SCREENSHOT_DELEGATE_H_

#include "base/callback_forward.h"
#include "base/optional.h"

namespace gfx {
class Image;
}

namespace chromeos {
namespace bloom {

// Prompts the user to select a part of the screen, and invokes the callback
// with a screenshot of the selected area.
class BloomScreenshotDelegate {
 public:
  using Callback = base::OnceCallback<void(base::Optional<gfx::Image>)>;

  virtual ~BloomScreenshotDelegate() = default;

  virtual void TakeScreenshot(Callback ready_callback) = 0;
};

}  // namespace bloom
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_BLOOM_PUBLIC_CPP_BLOOM_SCREENSHOT_DELEGATE_H_
