// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_ENUMS_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_ENUMS_H_

// The reason `AutocompleteController::Stop()` or `AutocompleteProvider::Stop()`
// were called.
enum class AutocompleteStopReason {

  // - Called when: Significant change occurred and the existing state is no
  //   longer useful.
  // - Action taken: Stop all providers and clear results.
  // - Example calls: The omnibox popup is closed or when some providers
  //   `Start()`.
  kClobbered,

  // - Called when: Omnibox was interacted with in a way that requires its state
  //   to be frozen.
  // - Action taken: Stop all providers and keep results.
  // - Example calls: Entered/left keyword mode, arrowed down through
  //   suggestions, arrowed left through text, match deleted , or when some
  //   providers `Start()`.
  kInteraction,

  // - Called when: User hasn't interacted with the omnibox for a while. See
  //   `AutocompleteController::stop_timer_duration_` comment.
  // - Action taken: Stop most providers and keep results. Some providers are
  //   allowed to continue if there's a good reason the user is likely to want
  //   even long-delayed asynchronous results, e.g. the user has explicitly
  //   invoked a keyword extension and the extension is still processing the
  //   request.
  // - Example calls: `stop_timer_` triggers.
  kInactivity,
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_ENUMS_H_
