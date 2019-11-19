// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SEARCHBOX_SEARCHBOX_EXTENSION_H_
#define CHROME_RENDERER_SEARCHBOX_SEARCHBOX_EXTENSION_H_

#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/common/search.mojom.h"
#include "ui/gfx/color_palette.h"

namespace blink {
class WebLocalFrame;
}

constexpr SkColor kNTPLightLogoColor = SkColorSetRGB(238, 238, 238);
constexpr SkColor kNTPLightIconColor = gfx::kGoogleGrey100;
constexpr SkColor kNTPDarkIconColor = gfx::kGoogleGrey900;

// Javascript bindings for the chrome.embeddedSearch APIs. See
// https://www.chromium.org/embeddedsearch.
class SearchBoxExtension {
 public:
  static void Install(blink::WebLocalFrame* frame);

  // Helpers to dispatch Javascript events.
  static void DispatchChromeIdentityCheckResult(blink::WebLocalFrame* frame,
                                                const base::string16& identity,
                                                bool identity_match);
  static void DispatchFocusChange(blink::WebLocalFrame* frame);
  static void DispatchAddCustomLinkResult(blink::WebLocalFrame* frame,
                                          bool success);
  static void DispatchUpdateCustomLinkResult(blink::WebLocalFrame* frame,
                                             bool success);
  static void DispatchDeleteCustomLinkResult(blink::WebLocalFrame* frame,
                                             bool success);
  static void DispatchAutocompleteResultChanged(
      blink::WebLocalFrame* frame,
      chrome::mojom::AutocompleteResultPtr result);
  static void DispatchInputCancel(blink::WebLocalFrame* frame);
  static void DispatchInputStart(blink::WebLocalFrame* frame);
  static void DispatchKeyCaptureChange(blink::WebLocalFrame* frame);
  static void DispatchMostVisitedChanged(blink::WebLocalFrame* frame);
  static void DispatchThemeChange(blink::WebLocalFrame* frame);
  static void DispatchLocalBackgroundSelected(blink::WebLocalFrame* frame);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SearchBoxExtension);
};

#endif  // CHROME_RENDERER_SEARCHBOX_SEARCHBOX_EXTENSION_H_
