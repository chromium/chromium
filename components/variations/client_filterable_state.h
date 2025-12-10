// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_CLIENT_FILTERABLE_STATE_H_
#define COMPONENTS_VARIATIONS_CLIENT_FILTERABLE_STATE_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/variations/proto/study.pb.h"

namespace variations {

// The values of the ChromeVariations policy. Those should be kept in sync
// with the values defined in policy_templates.json!
enum class RestrictionPolicy {
  // No restrictions applied by policy. Default value when policy not set.
  NO_RESTRICTIONS = 0,
  // Only critical security variations should be applied.
  CRITICAL_ONLY = 1,
  // All variations disabled. Disables the variations framework altogether.
  ALL = 2,
  kMaxValue = ALL,
};

using IsEnterpriseFunction = base::OnceCallback<bool()>;
using GoogleGroupsFunction = base::OnceCallback<base::flat_set<uint64_t>()>;

// A container for all of the client state which is used for filtering studies.
struct COMPONENT_EXPORT(VARIATIONS) ClientFilterableState {
  // The system locale.
  std::string locale;

  // The date on which the variations seed was fetched.
  base::Time reference_date;

  // The Chrome version to filter on.
  base::Version version;

  // The OS version to filter on. See |min_os_version| in study.proto for
  // details.
  base::Version os_version;

  // The Channel for this Chrome installation.
  Study::Channel channel = Study::UNKNOWN;

  // The hardware form factor that Chrome is running on.
  Study::FormFactor form_factor = Study::DESKTOP;

  // The CPU architecture on which Chrome is running.
  Study::CpuArchitecture cpu_architecture = Study::X86_64;

  // The OS on which Chrome is running.
  Study::Platform platform = Study::PLATFORM_WINDOWS;

  // The named hardware configuration that Chrome is running on -- used to
  // identify models of devices.
  std::string hardware_class;

  // Whether this is a low-end device. Currently only supported on Android.
  // Based on base::SysInfo::IsLowEndDevice().
  bool is_low_end_device = false;

  // The country code to use for studies configured with session consistency.
  std::string session_consistency_country;

  // The country code to use for studies configured with permanent consistency.
  std::string permanent_consistency_country;

  // The restriction applied to Chrome through the "ChromeVariations" policy.
  RestrictionPolicy policy_restriction = RestrictionPolicy::NO_RESTRICTIONS;

  explicit ClientFilterableState(IsEnterpriseFunction is_enterprise_function,
                                 GoogleGroupsFunction google_groups_function);

  ClientFilterableState(const ClientFilterableState&) = delete;
  ClientFilterableState& operator=(const ClientFilterableState&) = delete;

  ~ClientFilterableState();

  // Whether this is an enterprise client. Always false on Android, iOS, and
  // Linux. Determined by VariationsServiceClient::IsEnterprise() for Windows,
  // ChromeOS, and Mac.
  bool IsEnterprise() const;

  // The list of Google groups that one or more signed-in syncing users are a
  // a member of. Each value is the Gaia ID of the Google group.
  base::flat_set<uint64_t> GoogleGroups() const;

  static Study::Platform GetCurrentPlatform();

  // base::Version used in {min,max}_os_version filtering.
  static base::Version GetOSVersion();

  // Returns the hardware class string used for hardware_class filtering.
  static std::string GetHardwareClass();

 private:
  // Evaluating enterprise status negatively affects performance, so we only
  // evaluate it if needed (i.e. if a study is filtering by enterprise) and at
  // most once.
  mutable IsEnterpriseFunction is_enterprise_function_;
  mutable std::optional<bool> is_enterprise_;

  // Evaluating group memberships involves parsing data received from Chrome
  // Sync server.  For safe rollout we do this only for studies that require
  // inspecting group memberships (and for efficiency we do it only once.)
  mutable GoogleGroupsFunction google_groups_function_;
  mutable std::optional<base::flat_set<uint64_t>> google_groups_;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_CLIENT_FILTERABLE_STATE_H_
