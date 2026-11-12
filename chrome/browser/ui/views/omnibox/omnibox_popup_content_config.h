// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_CONTENT_CONFIG_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_CONTENT_CONFIG_H_

#include <optional>

#include "base/time/time.h"

// Per-flavor popup policy, read from feature flags once when the presenter is
// constructed. Production flags are fixed per process.
struct OmniboxPopupContentConfig {
  bool evict_on_hide = false;
  bool detach_on_hide = false;
  bool draw_shadow_in_webui = false;
  bool debounce_resize = false;
  bool height_workarounds = false;
  bool size_webview_to_preferred_height = false;
  std::optional<base::TimeDelta> defer_until_visual_state;

  static OmniboxPopupContentConfig ForClassic();
  static OmniboxPopupContentConfig ForFull();
  static OmniboxPopupContentConfig ForFullInLocationBar();
  static OmniboxPopupContentConfig ForAim();
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_CONTENT_CONFIG_H_
