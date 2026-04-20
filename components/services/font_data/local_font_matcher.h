// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_FONT_DATA_LOCAL_FONT_MATCHER_H_
#define COMPONENTS_SERVICES_FONT_DATA_LOCAL_FONT_MATCHER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"

namespace font_data_service {

// Result of a successful local font match: the on-disk file path and TTC index.
struct LocalFontMatchResult {
  base::FilePath file_path;
  int ttc_index = 0;
};

// Platform-agnostic interface for matching local fonts by PostScript name or
// full font name, as used by @font-face src: local(). Platform-specific
// implementations (e.g. DWriteLocalFontMatcher on Windows) provide the actual
// font lookup.
class LocalFontMatcher {
 public:
  static std::unique_ptr<LocalFontMatcher> Create();

  virtual ~LocalFontMatcher() = default;

  virtual std::optional<LocalFontMatchResult> MatchLocalFont(
      const std::string& font_unique_name) = 0;
};

}  // namespace font_data_service

#endif  // COMPONENTS_SERVICES_FONT_DATA_LOCAL_FONT_MATCHER_H_
