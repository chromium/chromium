// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_PROTOCOL_SERIALIZER_H_
#define COMPONENTS_UPDATE_CLIENT_PROTOCOL_SERIALIZER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/update_client/protocol_definition.h"

namespace base {
class Version;
}

namespace update_client {

class PersistedData;

// Creates the values for the DDOS extra request headers sent with the update
// check. These headers include "X-Goog-Update-Updater",
// "X-Goog-Update-AppId", and  "X-Goog-Update-Interactivity".
base::flat_map<std::string, std::string> BuildUpdateCheckExtraRequestHeaders(
    const std::string& prod_id,
    const base::Version& browser_version,
    const std::vector<std::string>& ids_checked,
    bool is_foreground);

protocol_request::Request MakeProtocolRequest(
    const std::string& session_id,
    const std::string& prod_id,
    const std::string& browser_version,
    const std::string& lang,
    const std::string& channel,
    const std::string& os_long_name,
    const std::string& download_preference,
    const base::flat_map<std::string, std::string>& additional_attributes,
    const std::map<std::string, std::string>* updater_state_attributes,
    std::vector<protocol_request::App> apps);

protocol_request::App MakeProtocolApp(
    const std::string& app_id,
    const base::Version& version,
    base::Optional<std::vector<base::Value>> events);

protocol_request::App MakeProtocolApp(
    const std::string& app_id,
    const base::Version& version,
    const std::string& brand_code,
    const std::string& install_source,
    const std::string& install_location,
    const std::string& fingerprint,
    const base::flat_map<std::string, std::string>& installer_attributes,
    const std::string& cohort,
    const std::string& cohort_hint,
    const std::string& cohort_name,
    const std::string& release_channel,
    const std::vector<int>& disabled_reasons,
    base::Optional<protocol_request::UpdateCheck> update_check,
    base::Optional<protocol_request::Ping> ping);

protocol_request::UpdateCheck MakeProtocolUpdateCheck(bool is_update_disabled);

protocol_request::Ping MakeProtocolPing(const std::string& app_id,
                                        const PersistedData* metadata);

class ProtocolSerializer {
 public:
  virtual ~ProtocolSerializer() = default;
  virtual std::string Serialize(
      const protocol_request::Request& request) const = 0;

 protected:
  ProtocolSerializer() = default;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_PROTOCOL_SERIALIZER_H_
