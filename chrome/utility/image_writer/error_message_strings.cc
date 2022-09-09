// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/image_writer/error_message_strings.h"

#include "build/build_config.h"

namespace image_writer {
namespace error {

const char kInvalidDevice[] = "Invalid device path.";
const char kOpenDevice[] = "Failed to open device.";
const char kOpenImage[] = "Failed to open image.";
const char kOperationAlreadyInProgress[] = "Operation already in progress.";
const char kReadDevice[] = "Failed to read device.";
const char kReadImage[] = "Failed to read image.";
const char kWriteImage[] = "Writing image to device failed.";
#if BUILDFLAG(IS_MAC)
const char kUnmountVolumes[] = "Unable to unmount the device.";
#endif
const char kVerificationFailed[] = "Verification failed.";

}  // namespace error
}  // namespace image_writer
