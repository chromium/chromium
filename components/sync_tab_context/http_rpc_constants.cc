// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_tab_context/http_rpc_constants.h"

#include "base/command_line.h"

namespace sync_tab_context {

const char kDefaultEphemeralKeyServerUrl[] =
    "https://chromesyncephemeralkeys.pa.googleapis.com/v1/ephemeralKeys";

const char kEphemeralKeyServerUrlSwitch[] =
    "tab-context-ephemeral-key-server-url";

const char kDefaultTabContextAllowedOrigin[] = "https://chromestorage.goog";

const char kTabContextAllowedOriginSwitch[] = "tab-context-allowed-origin";

GURL GetEphemeralKeyServerUrl() {
  const base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line && command_line->HasSwitch(kEphemeralKeyServerUrlSwitch)) {
    const GURL url(
        command_line->GetSwitchValueASCII(kEphemeralKeyServerUrlSwitch));
    if (url.is_valid()) {
      return url;
    }
  }
  return GURL(kDefaultEphemeralKeyServerUrl);
}

url::Origin GetAllowedTabContextOrigin() {
  const base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line && command_line->HasSwitch(kTabContextAllowedOriginSwitch)) {
    const GURL url(
        command_line->GetSwitchValueASCII(kTabContextAllowedOriginSwitch));
    if (url.is_valid()) {
      return url::Origin::Create(url);
    }
  }
  return url::Origin::Create(GURL(kDefaultTabContextAllowedOrigin));
}

}  // namespace sync_tab_context
