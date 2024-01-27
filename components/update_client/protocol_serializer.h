// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_PROTOCOL_SERIALIZER_H_
#define COMPONENTS_UPDATE_CLIENT_PROTOCOL_SERIALIZER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/values.h"
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
    bool is_machine,
    const std::string& session_id,
    const std::string& prod_id,
    const std::string& browser_version,
    const std::string& channel,
    const std::string& os_long_name,
    const std::string& download_preference,
    std::optional<bool> domain_joined,
    const base::flat_map<std::string, std::string>& additional_attributes,
    const base::flat_map<std::string, std::string>& updater_state_attributes,
    std::vector<protocol_request::App> apps);

protocol_request::App MakeProtocolApp(
    const std::string& app_id,
    const base::Version& version,
    const std::string& ap,
    const std::string& brand_code,
    const std::string& lang,
    int install_date,
    const std::string& install_source,
    const std::string& install_location,
    const std::string& fingerprint,
    const std::map<std::string, std::string>& installer_attributes,
    const std::string& cohort,
    const std::string& cohort_hint,
    const std::string& cohort_name,
    const std::string& release_channel,
    const std::vector<int>& disabled_reasons,
    std::optional<protocol_request::UpdateCheck> update_check,
    const std::vector<protocol_request::Data>& data,
    std::optional<protocol_request::Ping> ping,
    std::optional<std::vector<base::Value::Dict>> events);

protocol_request::UpdateCheck MakeProtocolUpdateCheck(
    bool is_update_disabled,
    const std::string& target_version_prefix,
    bool rollback_allowed,
    bool same_version_update_allowed);

protocol_request::Ping MakeProtocolPing(const std::string& app_id,
                                        const PersistedData* metadata,
                                        bool active);

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
