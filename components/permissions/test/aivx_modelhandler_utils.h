// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_TEST_AIVX_MODELHANDLER_UTILS_H_
#define COMPONENTS_PERMISSIONS_TEST_AIVX_MODELHANDLER_UTILS_H_

#include <memory>
#include <string>

#include "base/path_service.h"
#include "third_party/skia/include/core/SkBitmap.h"

// Contains methods that are helpful in the context of the AIvX model handler
// tests.
namespace test {

// Returns the test data file path for the respective AIvX model file.
base::FilePath ModelFilePath(std::string_view file_name);

// Returns a SkBitmap with the dimensions defined by width and height, filled
// with the provided color.
SkBitmap BuildBitmap(int width, int height, SkColor color);
}  // namespace test

#endif  // COMPONENTS_PERMISSIONS_TEST_AIVX_MODELHANDLER_UTILS_H_
