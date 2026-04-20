// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_FONT_DATA_DWRITE_LOCAL_FONT_MATCHER_H_
#define COMPONENTS_SERVICES_FONT_DATA_DWRITE_LOCAL_FONT_MATCHER_H_

#include <dwrite_3.h>
#include <wrl/client.h>

#include <string>

#include "components/services/font_data/local_font_matcher.h"

namespace font_data_service {

// Windows implementation of LocalFontMatcher using DirectWrite font set
// filtering.
class DWriteLocalFontMatcher : public LocalFontMatcher {
 public:
  DWriteLocalFontMatcher();
  ~DWriteLocalFontMatcher() override;

  DWriteLocalFontMatcher(const DWriteLocalFontMatcher&) = delete;
  DWriteLocalFontMatcher& operator=(const DWriteLocalFontMatcher&) = delete;

  std::optional<LocalFontMatchResult> MatchLocalFont(
      const std::string& font_unique_name) override;

 private:
  void EnsureDirectWriteInitialized();

  // Populates system_font_set_ with the system font set, including sideloaded
  // test fonts if any have been registered via SideLoadFontForTesting().
  void GetLocalFontSet(const Microsoft::WRL::ComPtr<IDWriteFactory3>& factory3);

  // Extracts the file path and TTC index from the first font in the filtered
  // set, or returns std::nullopt on failure.
  std::optional<LocalFontMatchResult> GetFileInfoFromFilteredSet(
      const Microsoft::WRL::ComPtr<IDWriteFontSet>& filtered_set);

  bool direct_write_initialized_ = false;
  Microsoft::WRL::ComPtr<IDWriteFontSet> system_font_set_;
};

}  // namespace font_data_service

#endif  // COMPONENTS_SERVICES_FONT_DATA_DWRITE_LOCAL_FONT_MATCHER_H_
