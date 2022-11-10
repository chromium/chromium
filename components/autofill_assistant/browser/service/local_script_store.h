// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_LOCAL_SCRIPT_STORE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_LOCAL_SCRIPT_STORE_H_

#include <string>
#include <vector>

#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

class LocalScriptStore {
 public:
  ~LocalScriptStore();
  LocalScriptStore(const LocalScriptStore&);
  LocalScriptStore(LocalScriptStore&&);

  LocalScriptStore(
      const std::vector<GetNoRoundTripScriptsByHashPrefixResponseProto::
                            MatchInfo::RoutineScript>& routines,
      const std::string& domain,
      const SupportsScriptResponseProto& supports_site_response);

  // Returns routines aka pairs of [script_path, ClientActionsResponseProto].
  [[nodiscard]] const std::vector<
      GetNoRoundTripScriptsByHashPrefixResponseProto::MatchInfo::RoutineScript>
  GetRoutines() const;

  // Returns the domain that this LocalScriptStore is valid for.
  [[nodiscard]] const std::string& GetDomain() const;

  // Returns the results of SupportsScript for this domain/Intent match.
  [[nodiscard]] const SupportsScriptResponseProto GetSupportsSiteResponse()
      const;

  // Returns if the store is empty (checks if the domains and routines are set).
  bool empty() const;

  // Returns the number of scripts in the store.
  size_t size() const;

 private:
  // Contains pairs of [script_path, ClientActionsResponseProto].
  std::vector<
      GetNoRoundTripScriptsByHashPrefixResponseProto::MatchInfo::RoutineScript>
      routines_;

  // The domain that this LocalScriptStore is valid for.
  std::string domain_;

  // The results of SupportsScript for this domain/Intent match.
  SupportsScriptResponseProto supports_site_response_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_LOCAL_SCRIPT_STORE_H_
