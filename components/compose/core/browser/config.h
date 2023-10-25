// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPOSE_CORE_BROWSER_CONFIG_H_
#define COMPONENTS_COMPOSE_CORE_BROWSER_CONFIG_H_

namespace compose {

// The Compose configuration. Default values appear below. Always use
// |GetComposeConfig()| to get the current configuration.
struct Config {
  // The minimum number of words needed for a valid user input.
  int input_min_words = 3;
  // The maximum number of words allowed for a valid user input.
  int input_max_words = 500;
  // The maximum number of characters allowed for a valid user input.
  int input_max_chars = 2500;

  Config();
  Config(const Config& other);
  ~Config();
};

// Gets the current configuration.
const Config& GetComposeConfig();

void SetComposeConfigForTesting(const Config& config);
void OverrideFieldTrialParamsForTesting();

}  // namespace compose

#endif  // COMPONENTS_FEED_CORE_V2_CONFIG_H_
