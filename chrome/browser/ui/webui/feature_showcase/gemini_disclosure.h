// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_GEMINI_DISCLOSURE_H_
#define CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_GEMINI_DISCLOSURE_H_

#include <string>
#include <string_view>

struct GeminiDisclosure {
  std::u16string first_paragraph;
  std::u16string second_paragraph;
  std::u16string third_paragraph;
};

// Returns the dynamically formatted Gemini consent strings depending on the
// user's |country_code| and whether the device |is_managed|.
GeminiDisclosure GetGeminiDisclosure(std::string_view country_code,
                                     bool is_managed);

#endif  // CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_GEMINI_DISCLOSURE_H_
