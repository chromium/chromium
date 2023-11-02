// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service/server_url_fetcher.h"

#include <string>

#include "base/command_line.h"
#include "components/autofill_assistant/browser/switches.h"
#include "url/url_canon_stdstring.h"

namespace {
const char kDefaultAutofillAssistantServerUrl[] =
    "https://automate-pa.googleapis.com";
const char kScriptEndpoint[] = "/v1/supportsSite2";
const char kActionEndpoint[] = "/v1/actions2";
const char kTriggersEndpoint[] = "/v1/triggers";
const char kTriggersByHashPrefixEndpoint[] = "/v1/triggerScriptsByHashPrefix";
const char kCapabilitiesByHashEndpoint[] = "/v1/capabilitiesByHashPrefix2";
const char kUserDataEndpoint[] = "/v1/userData";
const char kProgressEndpoint[] = "/v1/reportProgress";
const char kSelfContainedByHashEndpoint[] =
    "/v1/noRoundTripScriptsByHashPrefix";
}  // namespace

namespace autofill_assistant {

ServerUrlFetcher::ServerUrlFetcher(const GURL& server_url)
    : server_url_(server_url) {}
ServerUrlFetcher::~ServerUrlFetcher() = default;

// static
GURL ServerUrlFetcher::GetDefaultServerUrl() {
  std::string server_url =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAutofillAssistantUrl);
  if (server_url.empty()) {
    return GURL(kDefaultAutofillAssistantServerUrl);
  }
  return GURL(server_url);
}

bool ServerUrlFetcher::IsProdEndpoint() const {
  return server_url_ == GURL(kDefaultAutofillAssistantServerUrl);
}

GURL ServerUrlFetcher::GetSupportsScriptEndpoint() const {
  GURL::Replacements script_replacements;
  script_replacements.SetPathStr(kScriptEndpoint);
  return server_url_.ReplaceComponents(script_replacements);
}

GURL ServerUrlFetcher::GetNextActionsEndpoint() const {
  GURL::Replacements action_replacements;
  action_replacements.SetPathStr(kActionEndpoint);
  return server_url_.ReplaceComponents(action_replacements);
}

GURL ServerUrlFetcher::GetTriggerScriptsEndpoint() const {
  GURL::Replacements trigger_replacements;
  trigger_replacements.SetPathStr(kTriggersEndpoint);
  return server_url_.ReplaceComponents(trigger_replacements);
}

GURL ServerUrlFetcher::GetTriggerScriptsByHashPrefixEndpoint() const {
  GURL::Replacements trigger_replacements;
  trigger_replacements.SetPathStr(kTriggersByHashPrefixEndpoint);
  return server_url_.ReplaceComponents(trigger_replacements);
}

GURL ServerUrlFetcher::GetCapabilitiesByHashEndpoint() const {
  GURL::Replacements capabilities_replacements;
  capabilities_replacements.SetPathStr(kCapabilitiesByHashEndpoint);
  return server_url_.ReplaceComponents(capabilities_replacements);
}

GURL ServerUrlFetcher::GetUserDataEndpoint() const {
  GURL::Replacements user_data_replacements;
  user_data_replacements.SetPathStr(kUserDataEndpoint);
  return server_url_.ReplaceComponents(user_data_replacements);
}

GURL ServerUrlFetcher::GetReportProgressEndpoint() const {
  GURL::Replacements trigger_replacements;
  trigger_replacements.SetPathStr(kProgressEndpoint);
  return server_url_.ReplaceComponents(trigger_replacements);
}

GURL ServerUrlFetcher::GetNoRoundTripScriptsByHashEndpoint() const {
  GURL::Replacements no_roundtrip_hash_replacements;
  no_roundtrip_hash_replacements.SetPathStr(kSelfContainedByHashEndpoint);
  return server_url_.ReplaceComponents(no_roundtrip_hash_replacements);
}

}  // namespace autofill_assistant
