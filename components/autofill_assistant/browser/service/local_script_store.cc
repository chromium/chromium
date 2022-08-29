// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service/local_script_store.h"

#include <memory>
#include <numeric>

#include "base/bind.h"
#include "base/command_line.h"
#include "components/autofill_assistant/browser/protocol_utils.h"
#include "components/autofill_assistant/browser/public/autofill_assistant.h"
#include "components/autofill_assistant/browser/script_parameters.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/service/api_key_fetcher.h"
#include "components/autofill_assistant/browser/service/service_request_sender_impl.h"
#include "components/autofill_assistant/browser/switches.h"
#include "components/version_info/version_info.h"
#include "net/http/http_status_code.h"
#include "url/origin.h"

namespace autofill_assistant {

LocalScriptStore::LocalScriptStore(
    std::vector<GetNoRoundTripScriptsByHashPrefixResponseProto::MatchInfo::
                    RoutineScript> routines,
    std::string domain,
    SupportsScriptResponseProto supports_site_response)
    : routines_(routines),
      domain_(domain),
      supports_site_response_(supports_site_response) {}

LocalScriptStore::~LocalScriptStore() = default;

const std::vector<
    GetNoRoundTripScriptsByHashPrefixResponseProto::MatchInfo::RoutineScript>
LocalScriptStore::GetRoutines() const {
  return routines_;
}

const std::string LocalScriptStore::GetDomain() const {
  return domain_;
}

const SupportsScriptResponseProto LocalScriptStore::GetSupportsSiteResponse()
    const {
  return supports_site_response_;
}

bool LocalScriptStore::empty() const {
  return routines_.empty() || domain_.empty();
}

size_t LocalScriptStore::size() const {
  return domain_.empty() ? 0 : routines_.size();
}

}  // namespace autofill_assistant
