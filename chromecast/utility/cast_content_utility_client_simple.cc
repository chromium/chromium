// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/utility/cast_content_utility_client.h"

namespace chromecast {
namespace shell {

// static
std::unique_ptr<CastContentUtilityClient> CastContentUtilityClient::Create() {
  return std::make_unique<CastContentUtilityClient>();
}

}  // namespace shell
}  // namespace chromecast
