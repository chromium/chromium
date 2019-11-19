// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/extensions/api/command_line_private/command_line_private_api.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/values.h"
#include "chromecast/common/extensions_api/command_line_private.h"

namespace extensions {

namespace command_line_private = cast::api::command_line_private;

ExtensionFunction::ResponseAction CommandLinePrivateHasSwitchFunction::Run() {
  // This API is stubbed on chromecast to always return false.
  return RespondNow(
      ArgumentList(command_line_private::HasSwitch::Results::Create(false)));
}

}  // namespace extensions
