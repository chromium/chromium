// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_FONT_FONTCONFIG_MATCHING_H_
#define COMPONENTS_SERVICES_FONT_FONTCONFIG_MATCHING_H_

#include <optional>

#include "base/files/file_path.h"

namespace font_service {
// Searches FontConfig for a system font uniquely identified by full font name
// or postscript name. The matching algorithm tries to match both. Used for
// matching @font-face { src: local() } references in Blink.
class FontConfigLocalMatching {
 public:
  struct FontConfigMatchResult {
    base::FilePath file_path;
    unsigned ttc_index;
  };

  static std::optional<FontConfigMatchResult>
  FindFontByPostscriptNameOrFullFontName(const std::string& font_name);

 private:
  static std::optional<FontConfigMatchResult> FindFontBySpecifiedName(
      const char* fontconfig_parameter_name,
      const std::string& font_name);
};

}  // namespace font_service

#endif  // COMPONENTS_SERVICES_FONT_FONTCONFIG_MATCHING_H_
