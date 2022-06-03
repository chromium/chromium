// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service/api_key_fetcher.h"

#include "base/command_line.h"
#include "components/autofill_assistant/browser/switches.h"
#include "google_apis/google_api_keys.h"

namespace autofill_assistant {

std::string ApiKeyFetcher::GetAPIKey(version_info::Channel channel) {
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAutofillAssistantServerKey)) {
    return command_line->GetSwitchValueASCII(
        switches::kAutofillAssistantServerKey);
  }

  if (google_apis::IsGoogleChromeAPIKeyUsed()) {
    return channel == version_info::Channel::STABLE
               ? google_apis::GetAPIKey()
               : google_apis::GetNonStableAPIKey();
  }
  return "";
}

}  // namespace autofill_assistant
