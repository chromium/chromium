// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v4_protocol_config.h"

#include <string>

#include "components/version_info/version_info.h"
#include "google_apis/google_api_keys.h"

namespace safe_browsing {

V4ProtocolConfig::V4ProtocolConfig(const std::string& client_name,
                                   bool disable_auto_update,
                                   const std::string& key_param,
                                   const std::string& version)
    : client_name(client_name),
      disable_auto_update(disable_auto_update),
      key_param(key_param),
      version(version) {}

V4ProtocolConfig::V4ProtocolConfig(const V4ProtocolConfig& other) = default;

V4ProtocolConfig::~V4ProtocolConfig() = default;

V4ProtocolConfig GetV4ProtocolConfig(const std::string& client_name,
                                     bool disable_auto_update) {
  return V4ProtocolConfig(client_name, disable_auto_update,
                          google_apis::GetAPIKey(),
                          std::string(version_info::GetVersionNumber()));
}

}  // namespace safe_browsing
