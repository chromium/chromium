// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SPARKY_SNAPSHOT_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_SPARKY_SNAPSHOT_UTIL_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"

namespace sparky {

using ScreenshotDataCallback =
    base::OnceCallback<void(scoped_refptr<base::RefCountedMemory>)>;

// Handles taking a screenshot of the open windows.
// Encodes the screenshot as jpg and returns it with the callback.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SPARKY) ScreenshotHandler {
 public:
  ScreenshotHandler();
  ScreenshotHandler(const ScreenshotHandler&) = delete;
  ScreenshotHandler& operator=(const ScreenshotHandler&) = delete;
  ~ScreenshotHandler();

  // Takes a screenshot of the windows on the active desk and writes it to disk.
  // Invokes `done_callback` when done.
  void TakeScreenshot(ScreenshotDataCallback done_callback);
};

}  // namespace sparky

#endif  // CHROMEOS_ASH_COMPONENTS_SPARKY_SNAPSHOT_UTIL_H_
