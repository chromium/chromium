// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V4_PROTOCOL_CONFIG_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V4_PROTOCOL_CONFIG_H_

#include <string>

namespace safe_browsing {

// Config passed to the constructor of a V4 protocol manager.
struct V4ProtocolConfig {
  // The safe browsing client name sent in each request.
  std::string client_name;

  // Disable auto-updates using a command line switch.
  bool disable_auto_update;

  // The Google API key.
  std::string key_param;

  // Current product version sent in each request.
  std::string version;

  V4ProtocolConfig() = delete;
  V4ProtocolConfig(const std::string& client_name,
                   bool disable_auto_update,
                   const std::string& key_param,
                   const std::string& version);
  V4ProtocolConfig(const V4ProtocolConfig& other);
  ~V4ProtocolConfig();
};

// Get the v4 protocol config struct with a given client name, and ability to
// enable/disable database auto update.
V4ProtocolConfig GetV4ProtocolConfig(const std::string& client_name,
                                     bool disable_auto_update);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V4_PROTOCOL_CONFIG_H_
