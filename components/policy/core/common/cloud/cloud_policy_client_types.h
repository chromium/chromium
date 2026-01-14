// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CLIENT_TYPES_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CLIENT_TYPES_H_

#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "components/policy/policy_export.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

struct POLICY_EXPORT ExtensionInstallDecision {
  ExtensionInstallDecision();
  ExtensionInstallDecision(
      enterprise_management::ExtensionInstallPolicy::Action action,
      std::set<enterprise_management::ExtensionInstallPolicy::Reason> reasons);
  ExtensionInstallDecision(const ExtensionInstallDecision&);
  ExtensionInstallDecision(ExtensionInstallDecision&&);
  ~ExtensionInstallDecision();

  enterprise_management::ExtensionInstallPolicy::Action action =
      enterprise_management::ExtensionInstallPolicy::ACTION_ALLOW;
  std::set<enterprise_management::ExtensionInstallPolicy::Reason> reasons;
};

struct POLICY_EXPORT ExtensionIdAndVersion {
  std::string extension_id;
  std::string extension_version;

  bool operator<(const ExtensionIdAndVersion& other) const;
  bool operator==(const ExtensionIdAndVersion& other) const;

  std::string ToString() const;
};

class POLICY_EXPORT PolicyTypeToFetch {
 public:
  class ExtensionsProvider {
   public:
    virtual std::set<ExtensionIdAndVersion> GetExtensions() = 0;
  };

  using ExtensionsProviderRef = raw_ref<ExtensionsProvider>;

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
  PolicyTypeToFetch(const std::string& policy_type,
                    const std::string& settings_entity_id);

  PolicyTypeToFetch(const std::string& policy_type,
                    ExtensionsProvider* extension_set_provider);

  PolicyTypeToFetch(const PolicyTypeToFetch&);
  PolicyTypeToFetch(PolicyTypeToFetch&&);

  ~PolicyTypeToFetch();

  PolicyTypeToFetch& operator=(const PolicyTypeToFetch&);
  PolicyTypeToFetch& operator=(PolicyTypeToFetch&&);

  std::strong_ordering operator<=>(const PolicyTypeToFetch& other) const =
      default;

  const std::string& policy_type() const { return policy_type_; }
  std::string settings_entity_id() const;
  std::set<ExtensionIdAndVersion> extension_ids_and_version() const;

 private:
  std::string policy_type_;
  std::variant<std::string, ExtensionsProviderRef> extra_param_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CLIENT_TYPES_H_
