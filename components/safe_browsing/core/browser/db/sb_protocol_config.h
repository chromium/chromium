// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_SB_PROTOCOL_CONFIG_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_SB_PROTOCOL_CONFIG_H_

#include <string>

namespace safe_browsing {

// Config passed to the constructor of an SB protocol manager.
struct SBProtocolConfig {
  // The safe browsing client name sent in each request.
  std::string client_name;

  // Disable auto-updates using a command line switch.
  bool disable_auto_update;

  // The Google API key.
  std::string key_param;

  // Current product version sent in each request.
  std::string version;

  SBProtocolConfig() = delete;
  SBProtocolConfig(const std::string& client_name,
                   bool disable_auto_update,
                   const std::string& key_param,
                   const std::string& version);
  SBProtocolConfig(const SBProtocolConfig& other);
  ~SBProtocolConfig();
};

// Get the SB protocol config struct with a given client name, and ability to
// enable/disable database auto update.
SBProtocolConfig GetSBProtocolConfig(const std::string& client_name,
                                     bool disable_auto_update);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_SB_PROTOCOL_CONFIG_H_
