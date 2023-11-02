// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_HTTP_USER_AGENT_SETTINGS_H_
#define CHROMECAST_BROWSER_CAST_HTTP_USER_AGENT_SETTINGS_H_

#include <string>

#include "base/compiler_specific.h"

namespace chromecast {
namespace shell {

class CastHttpUserAgentSettings {
 public:
  CastHttpUserAgentSettings() = delete;
  CastHttpUserAgentSettings(const CastHttpUserAgentSettings&) = delete;
  CastHttpUserAgentSettings& operator=(const CastHttpUserAgentSettings&) =
      delete;

  ~CastHttpUserAgentSettings() = delete;

  // Returns the same value as GetAcceptLanguage(), but is static and can be
  // called on any thread.
  static std::string AcceptLanguage();
};

}  // namespace shell
}  // namespace chromecast
#endif  // CHROMECAST_BROWSER_CAST_HTTP_USER_AGENT_SETTINGS_H_
