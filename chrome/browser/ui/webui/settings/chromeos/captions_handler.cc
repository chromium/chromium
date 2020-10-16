// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/captions_handler.h"

#include "base/bind_helpers.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {
namespace settings {

CaptionsHandler::CaptionsHandler() = default;

CaptionsHandler::~CaptionsHandler() = default;

void CaptionsHandler::RegisterMessages() {
  // TODO(crbug.com/1111002): Show download progress of SODA component in the
  // Live Caption subtitle.
  web_ui()->RegisterMessageCallback("captionsSubpageReady", base::DoNothing());
}

}  // namespace settings
}  // namespace chromeos
