// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_TESTING_UTILS_H_
#define COMPONENTS_LENS_LENS_TESTING_UTILS_H_

#include <string>

namespace lens {
// For testing purposes, parses the encoded post data that is sent with a
// request, and returns the image bytes portion of the request. Returns an
// empty string if no image bytes are present.
std::string GetImageBytesFromEncodedPostData(const std::string& post_data);
}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_TESTING_UTILS_H_