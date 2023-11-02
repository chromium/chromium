// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ABOUT_UI_CREDIT_UTILS_H_
#define COMPONENTS_ABOUT_UI_CREDIT_UTILS_H_

#include <string>

namespace about_ui {

// Decode a Brotli compressed HTML license file and attach .js files.
std::string GetCredits(bool include_scripts);

}  // namespace about_ui

#endif  // COMPONENTS_ABOUT_UI_CREDIT_UTILS_H_
