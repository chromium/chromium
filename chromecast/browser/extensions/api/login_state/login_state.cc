// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/extensions/api/login_state/login_state.h"

namespace {
const char kErrorNotImplemented[] = "API not implemented";
}  // namespace

namespace extensions {

ExtensionFunction::ResponseAction LoginStateGetProfileTypeFunction::Run() {
  return RespondNow(Error(kErrorNotImplemented));
}

ExtensionFunction::ResponseAction LoginStateGetSessionStateFunction::Run() {
  return RespondNow(Error(kErrorNotImplemented));
}

}  // namespace extensions
