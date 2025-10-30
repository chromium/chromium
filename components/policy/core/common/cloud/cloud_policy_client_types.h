// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CLIENT_TYPES_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CLIENT_TYPES_H_

#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/policy/policy_export.h"

namespace policy {

struct POLICY_EXPORT ExtensionIdAndVersion {
  std::string extension_id;
  std::string extension_version;

  bool operator<(const ExtensionIdAndVersion& other) const;
  bool operator==(const ExtensionIdAndVersion& other) const;
};

class POLICY_EXPORT CloudPolicyClientTypeParams {
 public:
  // When used in a set, `policy_type` and |settings_entity_id| will be used
  // as the primary key. Only one of |settings_entity_id| and
  // `extension_ids_and_version|` is only used for the policy type
  // `google/extension-install-cloud-policy/chrome/*` and is formatted as
  // `{extension_id}@{extension_version}`.
  //   - When fetching the policy type
  //   `google/extension-install-cloud-policy/*`,
  //     for multiple extension ids and versions, `settings_entity_id` must be
  //     empty.
  //  -  When fetching for a single extension id and version,
  //  `settings_entity_id`
  //     must be also set to `{extension_id}@{extension_version}`.
  CloudPolicyClientTypeParams(const std::string& policy_type,
                              const std::string& settings_entity_id);

  CloudPolicyClientTypeParams(const std::string& policy_type,
                              ExtensionIdAndVersion extension_id_and_version);

  CloudPolicyClientTypeParams(
      const std::string& policy_type,
      base::RepeatingCallback<std::set<ExtensionIdAndVersion>()>
          extension_ids_and_version_getter);

  CloudPolicyClientTypeParams(const CloudPolicyClientTypeParams&);
  CloudPolicyClientTypeParams(CloudPolicyClientTypeParams&&);

  ~CloudPolicyClientTypeParams();

  CloudPolicyClientTypeParams& operator=(const CloudPolicyClientTypeParams&);
  CloudPolicyClientTypeParams& operator=(CloudPolicyClientTypeParams&&);

  bool operator<(const CloudPolicyClientTypeParams& other) const;
  bool operator==(const CloudPolicyClientTypeParams& other) const;

  const std::string& policy_type() const { return policy_type_; }
  const std::string& settings_entity_id() const { return settings_entity_id_; }
  std::set<ExtensionIdAndVersion> extension_ids_and_version() const;

 private:
  std::string policy_type_;
  std::string settings_entity_id_;
  base::RepeatingCallback<std::set<ExtensionIdAndVersion>()>
      extension_ids_and_version_getter_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CLIENT_TYPES_H_
