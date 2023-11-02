// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SEARCH_INSTANT_TYPES_H_
#define CHROME_COMMON_SEARCH_INSTANT_TYPES_H_

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "url/gurl.h"

// ID used by Instant code to refer to objects (e.g. Autocomplete results, Most
// Visited items) that the Instant page needs access to.
typedef int InstantRestrictedID;

// The alignment of the theme background image.
enum ThemeBackgroundImageAlignment {
  THEME_BKGRND_IMAGE_ALIGN_CENTER,
  THEME_BKGRND_IMAGE_ALIGN_LEFT,
  THEME_BKGRND_IMAGE_ALIGN_TOP,
  THEME_BKGRND_IMAGE_ALIGN_RIGHT,
  THEME_BKGRND_IMAGE_ALIGN_BOTTOM,

  THEME_BKGRND_IMAGE_ALIGN_LAST = THEME_BKGRND_IMAGE_ALIGN_BOTTOM,
};

// The tiling of the theme background image.
enum ThemeBackgroundImageTiling {
  THEME_BKGRND_IMAGE_NO_REPEAT,
  THEME_BKGRND_IMAGE_REPEAT_X,
  THEME_BKGRND_IMAGE_REPEAT_Y,
  THEME_BKGRND_IMAGE_REPEAT,

  THEME_BKGRND_IMAGE_LAST = THEME_BKGRND_IMAGE_REPEAT,
};

// Theme settings for the NTP.
struct NtpTheme {
  NtpTheme();
  NtpTheme(const NtpTheme& other);
  ~NtpTheme();

  bool operator==(const NtpTheme& rhs) const;

  // True if the default theme is selected.
  bool using_default_theme = true;

  // The theme background color. Always valid.
  SkColor background_color = gfx::kPlaceholderColor;

  // The theme text color.
  SkColor text_color = gfx::kPlaceholderColor;

  // The theme text color light.
  SkColor text_color_light = gfx::kPlaceholderColor;

  // The theme id for the theme background image.
  // Value is only valid if there's a custom theme background image.
  std::string theme_id;

  // The theme background image horizontal alignment is only valid if |theme_id|
  // is valid.
  ThemeBackgroundImageAlignment image_horizontal_alignment =
      THEME_BKGRND_IMAGE_ALIGN_CENTER;

  // The theme background image vertical alignment is only valid if |theme_id|
  // is valid.
  ThemeBackgroundImageAlignment image_vertical_alignment =
      THEME_BKGRND_IMAGE_ALIGN_CENTER;

  // The theme background image tiling is only valid if |theme_id| is valid.
  ThemeBackgroundImageTiling image_tiling = THEME_BKGRND_IMAGE_NO_REPEAT;

  // True if theme has attribution logo.
  // Value is only valid if |theme_id| is valid.
  bool has_attribution = false;

  // True if theme has an alternate logo.
  bool logo_alternate = false;

  // True if theme has NTP image.
  bool has_theme_image = false;
};

struct InstantMostVisitedItem {
  InstantMostVisitedItem();
  InstantMostVisitedItem(const InstantMostVisitedItem& other);
  ~InstantMostVisitedItem();

  // The URL of the Most Visited item.
  GURL url;

  // The title of the Most Visited page.  May be empty, in which case the |url|
  // is used as the title.
  std::u16string title;

  // The external URL of the favicon associated with this page.
  GURL favicon;
};

struct InstantMostVisitedInfo {
  InstantMostVisitedInfo();
  InstantMostVisitedInfo(const InstantMostVisitedInfo& other);
  ~InstantMostVisitedInfo();

  std::vector<InstantMostVisitedItem> items;
};

// An InstantMostVisitedItem along with its assigned restricted ID.
typedef std::pair<InstantRestrictedID, InstantMostVisitedItem>
    InstantMostVisitedItemIDPair;

#endif  // CHROME_COMMON_SEARCH_INSTANT_TYPES_H_
