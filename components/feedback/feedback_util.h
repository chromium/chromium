// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_FEEDBACK_UTIL_H_
#define COMPONENTS_FEEDBACK_FEEDBACK_UTIL_H_

#include <string>

#include "base/files/file_path.h"

namespace feedback_util {

bool ZipString(const base::FilePath& filename,
               const std::string& data,
               std::string* compressed_data);

// Returns true for google.com email addresses.
bool IsGoogleEmail(const std::string& email);

}  // namespace feedback_util

#endif  // COMPONENTS_FEEDBACK_FEEDBACK_UTIL_H_
