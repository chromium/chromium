// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_TRANSLATE_OPTIONS_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_TRANSLATE_OPTIONS_H_

#include <string>

namespace lens {

// Data struct representing options for translate data if set.
struct TranslateOptions {
  std::string source_language;
  std::string target_language;

  TranslateOptions(const std::string& source, const std::string& target)
      : source_language(source), target_language(target) {}
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_TRANSLATE_OPTIONS_H_
