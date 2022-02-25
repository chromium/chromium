// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_CONFIG_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_CONFIG_H_

#include <string>

namespace history_clusters {

// The default configuration. Always use |GetConfig()| to get the current
// configuration.
struct Config {
  //
  bool is_journeys_enabled = false;

  Config();
  Config(const Config& other);
  ~Config();
};

// Gets the current configuration. OverrideWithFinch() must have been called
// before GetConfig() is called.
const Config& GetConfig();

// Override any parameters that may be provided by Finch.
void OverrideWithFinch(const std::string& application_locale);

// Resets the static config object for testing.
void ResetConfigForTesting();

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_CONFIG_H_