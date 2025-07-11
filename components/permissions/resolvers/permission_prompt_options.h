// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_RESOLVERS_PERMISSION_PROMPT_OPTIONS_H_
#define COMPONENTS_PERMISSIONS_RESOLVERS_PERMISSION_PROMPT_OPTIONS_H_

#include <variant>

// This file defined PromptOptions for permission prompts. Prompt options are
// user options on a prompt, such as selecting approximate/precise location, and
// are consumed by the PermissionResolver.

// GeolocationPromptOptions are used for prompt options for geolocation.
struct GeolocationPromptOptions {
  // If the user is in the approximate geolocation experiment
  // (kApproximateGeolocationPermission), and the site requests a precise grant,
  // the user will be shown a prompt which allows the user to choose whether
  // they want to grant precise or approximate location. In this case, the
  // prompt will return an instance of this struct, where |granted_precise| is
  // set to what the user chose.
  bool selected_precise = false;
};

// PromptOptions can be passed back by the prompt if there was a user choice of
// options on the prompt. If the prompt doesn't offer prompt options, it can
// pass back std::monostate (default).
using PromptOptions = std::variant<GeolocationPromptOptions, std::monostate>;

#endif  // COMPONENTS_PERMISSIONS_RESOLVERS_PERMISSION_PROMPT_OPTIONS_H_
