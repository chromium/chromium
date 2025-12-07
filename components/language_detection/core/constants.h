// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_DETECTION_CORE_CONSTANTS_H_
#define COMPONENTS_LANGUAGE_DETECTION_CORE_CONSTANTS_H_

namespace language_detection {

// The language code used when the language of a page could not be detected.
// (Matches what the CLD -Compact Language Detection- library reports.)
inline constexpr char kUnknownLanguageCode[] = "und";

}  // namespace language_detection

#endif  // COMPONENTS_LANGUAGE_DETECTION_CORE_CONSTANTS_H_
