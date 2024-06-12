// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MANTA_PROVIDER_PARAMS_H_
#define COMPONENTS_MANTA_PROVIDER_PARAMS_H_

#include <string>

#include "base/version_info/channel.h"
namespace manta {

// Additional parameters the Manta server side may expect or require for certain
// providers.
struct ProviderParams {
  // If true, use API key instead of Gaia-based authentication.
  bool use_api_key = false;
  // If non-empty, include chrome version info in the request.
  std::string chrome_version;
  // Which Chrome channel to send in the request, or UNKNOWN by default.
  version_info::Channel chrome_channel = version_info::Channel::UNKNOWN;
  // If non-empty, include locale info in the request.
  std::string locale;
};

}  // namespace manta

#endif  // COMPONENTS_MANTA_PROVIDER_PARAMS_H_
