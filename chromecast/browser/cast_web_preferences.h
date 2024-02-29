// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_WEB_PREFERENCES_H_
#define CHROMECAST_BROWSER_CAST_WEB_PREFERENCES_H_

#include <optional>

#include "base/supports_user_data.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom.h"

namespace chromecast {

// Stores user provided settings for a specific WebContents. These will be used
// to override default WebPreferences during the lifetime of the WebContents.
class CastWebPreferences : public base::SupportsUserData::Data {
 public:
  struct Preferences {
    Preferences();

    std::optional<blink::mojom::AutoplayPolicy> autoplay_policy;
    std::optional<bool> hide_scrollbars;
    std::optional<bool> javascript_enabled;
    std::optional<bool> supports_multiple_windows;
  };

  // Unique key used to identify CastWebPreferences in WebContents' user data.
  static const void* kCastWebPreferencesDataKey;

  CastWebPreferences();

  CastWebPreferences(const CastWebPreferences&) = delete;
  CastWebPreferences& operator=(CastWebPreferences&) = delete;

  Preferences* preferences() { return &preferences_; }

  // Overrides |prefs| with any locally stored preferences.
  void Update(blink::web_pref::WebPreferences* prefs);

 private:
  Preferences preferences_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_WEB_PREFERENCES_H_
