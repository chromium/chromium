// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service/local_script_store.h"

#include <memory>
#include <numeric>

#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

LocalScriptStore::LocalScriptStore(
    const std::vector<GetNoRoundTripScriptsByHashPrefixResponseProto::
                          MatchInfo::RoutineScript>& routines,
    const std::string& domain,
    const SupportsScriptResponseProto& supports_site_response)
    : routines_(routines),
      domain_(domain),
      supports_site_response_(supports_site_response) {}

LocalScriptStore::~LocalScriptStore() = default;

const std::vector<
    GetNoRoundTripScriptsByHashPrefixResponseProto::MatchInfo::RoutineScript>
LocalScriptStore::GetRoutines() const {
  return routines_;
}

const std::string& LocalScriptStore::GetDomain() const {
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
