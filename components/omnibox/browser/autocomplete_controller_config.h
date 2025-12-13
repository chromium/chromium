// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_CONTROLLER_CONFIG_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_CONTROLLER_CONFIG_H_

#include "base/time/time.h"

// Used to configure `AutocomleteController` behavior for its different
// embedders. This should only store config values that are constant during the
// controller's lifetime.
struct AutocompleteControllerConfig {
  // Bitmap containing `AutocompleteProvider::Type`s that might, depending on
  // platform, flags, etc., be instantiated.
  int provider_types = 0;

  // Amount of time between when the user stops typing and when autocompletion
  // is stopped. This is intended to avoid the disruptive effect of belated
  // omnibox updates, updates that come after the user has had to time to read
  // the whole dropdown and doesn't expect it to change.
  base::TimeDelta stop_timer_duration = base::Milliseconds(1500);

  // Whether to show open-tab suggestions even when not in the @tabs scope. Used
  // by the CrOS launcher.
  bool unscoped_open_tab_suggestions = false;

  // Disables ML scoring regardless of its feature state. Used by
  // chrome://omnibox/ml.
  bool disable_ml = false;

  // Show IPH matches from the `FeaturedSearchProvider`. True for all embedders
  // except the WebUI omnibox popup, which hasn't implemented them yet.
  bool show_iph_matches = true;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_CONTROLLER_CONFIG_H_
