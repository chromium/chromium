// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_web_preferences.h"

namespace chromecast {

const void* CastWebPreferences::kCastWebPreferencesDataKey =
    &CastWebPreferences::kCastWebPreferencesDataKey;

CastWebPreferences::Preferences::Preferences() = default;

CastWebPreferences::CastWebPreferences() = default;

void CastWebPreferences::Update(blink::web_pref::WebPreferences* prefs) {
  if (preferences_.autoplay_policy)
    prefs->autoplay_policy = preferences_.autoplay_policy.value();

  if (preferences_.hide_scrollbars)
    prefs->hide_scrollbars = preferences_.hide_scrollbars.value();

  if (preferences_.javascript_enabled)
    prefs->javascript_enabled = preferences_.javascript_enabled.value();

  if (preferences_.supports_multiple_windows) {
    prefs->supports_multiple_windows = preferences_.supports_multiple_windows.value();
  }
}

}  // namespace chromecast
