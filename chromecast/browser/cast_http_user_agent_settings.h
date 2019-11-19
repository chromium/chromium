// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_HTTP_USER_AGENT_SETTINGS_H_
#define CHROMECAST_BROWSER_CAST_HTTP_USER_AGENT_SETTINGS_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "net/base/http_user_agent_settings.h"

namespace chromecast {
namespace shell {

class CastHttpUserAgentSettings : public net::HttpUserAgentSettings {
 public:
  CastHttpUserAgentSettings();
  ~CastHttpUserAgentSettings() override;

  // net::HttpUserAgentSettings implementation:
  std::string GetAcceptLanguage() const override;
  std::string GetUserAgent() const override;

  // Returns the same value as GetAcceptLanguage(), but is static and can be
  // called on any thread.
  static std::string AcceptLanguage();

 private:
  mutable std::string last_locale_;
  mutable std::string accept_language_;

  DISALLOW_COPY_AND_ASSIGN(CastHttpUserAgentSettings);
};

}  // namespace shell
}  // namespace chromecast
#endif  // CHROMECAST_BROWSER_CAST_HTTP_USER_AGENT_SETTINGS_H_
