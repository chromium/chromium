// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_BLOOM_SCREENSHOT_GRABBER_H_
#define CHROMEOS_COMPONENTS_BLOOM_SCREENSHOT_GRABBER_H_

#include <vector>
#include "base/callback_forward.h"
#include "base/optional.h"

namespace chromeos {
namespace bloom {

using Screenshot = std::vector<uint8_t>;

// Interface to grab a screenshot.
class ScreenshotGrabber {
 public:
  using Callback =
      base::OnceCallback<void(base::Optional<Screenshot> screenshot)>;
  virtual ~ScreenshotGrabber() = default;

  // Asynchronously takes a screenshot, and passes it to the callback.
  // If this fails or is aborted, will pass |base::nullopt| to the callback.
  virtual void TakeScreenshot(Callback callback) = 0;
};

}  // namespace bloom
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_BLOOM_SCREENSHOT_GRABBER_H_
