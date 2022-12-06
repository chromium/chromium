
// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_testing_utils.h"

#include "base/strings/string_util.h"

namespace lens {

std::string GetImageBytesFromEncodedPostData(const std::string& post_data) {
  static const char kImageDataStartString[] = "Content-Type: image/jpeg";
  static const char kImageDataEndString[] = "------MultipartBoundary";

  std::size_t image_data_start =
      post_data.find(kImageDataStartString) + strlen(kImageDataStartString);
  std::size_t image_data_end =
      post_data.find(kImageDataEndString, image_data_start);
  std::string image_data =
      post_data.substr(image_data_start, image_data_end - image_data_start);

  // Remove extra whitespace that gets added to the encoding
  return base::CollapseWhitespaceASCII(
      image_data, /* trim_sequences_with_line_breaks= */ true);
}

}  // namespace lens