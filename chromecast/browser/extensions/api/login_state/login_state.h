// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_EXTENSIONS_API_LOGIN_STATE_LOGIN_STATE_H_
#define CHROMECAST_BROWSER_EXTENSIONS_API_LOGIN_STATE_LOGIN_STATE_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

class LoginStateGetProfileTypeFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("loginState.getProfileType",
                             LOGINSTATE_GETPROFILETYPE)

 protected:
  ~LoginStateGetProfileTypeFunction() override {}
  ResponseAction Run() override;
};

class LoginStateGetSessionStateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("loginState.getSessionState",
                             LOGINSTATE_GETSESSIONSTATE)

 protected:
  ~LoginStateGetSessionStateFunction() override {}
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROMECAST_BROWSER_EXTENSIONS_API_LOGIN_STATE_LOGIN_STATE_H_
