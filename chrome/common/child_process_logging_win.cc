// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_logging.h"

#include <windows.h>

#include <memory>

#include "chrome/chrome_elf/chrome_elf_main.h"
#include "chrome/common/crash_keys.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/metrics/client_info.h"

namespace child_process_logging {

void Init() {
  // This would be handled by BreakpadClient::SetCrashClientIdFromGUID(), but
  // because of the aforementioned issue, crash keys aren't ready yet at the
  // time of Breakpad initialization, load the client id backed up in Google
  // Update settings instead.
  // Please note if we are using Crashpad via chrome_elf then we need to call
  // into chrome_elf to pass in the client id.
  std::unique_ptr<metrics::ClientInfo> client_info =
      GoogleUpdateSettings::LoadMetricsClientInfo();

  // Set the client id chrome_elf (in tests this is stubbed).
  SetMetricsClientId(client_info ? client_info->client_id.c_str() : nullptr);
}

}  // namespace child_process_logging
