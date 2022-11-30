// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_OMNIBOX_FORMATTING_H_
#define CHROME_BROWSER_VR_ELEMENTS_OMNIBOX_FORMATTING_H_

#include <memory>

#include "chrome/browser/vr/elements/text.h"
#include "chrome/browser/vr/model/color_scheme.h"
#include "chrome/browser/vr/ui_support.h"
#include "chrome/browser/vr/vr_ui_export.h"
#include "components/omnibox/browser/autocomplete_match.h"

namespace gfx {
class RenderText;
}

namespace vr {

// Convert Autocomplete's suggestion formatting to generic VR text formatting.
VR_UI_EXPORT TextFormatting
ConvertClassification(const ACMatchClassifications& classifications,
                      size_t text_length,
                      const ColorScheme& color_scheme);

struct ElisionParameters {
  // The horizontal pixel offset to be applied to URL text, such that the right
  // edge of the domain (along with at least a small amount of path) is visible
  // within the available text field space.
  int offset = 0;

  // Flags indicating that either edge of the URL overflows the ends of the text
  // field, after offsetting, and that fading must be applied.
  bool fade_left = false;
  bool fade_right = false;
};

// Based on a URL and the RenderText that will draw it, determine the required
// elision parameters.  This means computing an offset such that the rightmost
// portion of the TLD is visible (along with a small part of the path), and
// fading either edge if they overflow available space.
VR_UI_EXPORT ElisionParameters
GetElisionParameters(const GURL& gurl,
                     const url::Parsed& parsed,
                     gfx::RenderText* render_text,
                     int min_path_pixels);

// Given a formatted URL and associated Parsed data, generates a VR-specific
// text formatting description that can be applied to a RenderText.  This mainly
// handles emphasis of hosts, etc., but could also include color.
VR_UI_EXPORT TextFormatting
CreateUrlFormatting(const std::u16string& formatted_url,
                    const url::Parsed& parsed,
                    SkColor emphasized_color,
                    SkColor deemphasized_color);

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_OMNIBOX_FORMATTING_H_
