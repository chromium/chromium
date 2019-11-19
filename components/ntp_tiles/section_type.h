// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_SECTION_TYPE_H_
#define COMPONENTS_NTP_TILES_SECTION_TYPE_H_

namespace ntp_tiles {

// The type of a section means all its tiles originate here. Ranked descendingly
// from most important section to least important.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.suggestions.tile
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: TileSectionType
enum class SectionType {
  UNKNOWN,
  PERSONALIZED,
  SOCIAL,
  ENTERTAINMENT,
  NEWS,
  ECOMMERCE,
  TOOLS,
  TRAVEL,

  LAST = TRAVEL
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_SECTION_TYPE_H_
