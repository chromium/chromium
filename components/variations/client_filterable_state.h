// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_CLIENT_FILTERABLE_STATE_H_
#define COMPONENTS_VARIATIONS_CLIENT_FILTERABLE_STATE_H_

#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/variations/proto/study.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

// A container for all of the client state which is used for filtering studies.
struct COMPONENT_EXPORT(VARIATIONS) ClientFilterableState {
  static Study::Platform GetCurrentPlatform();

  // base::Version used in {min,max}_os_version filtering.
  static base::Version GetOSVersion();

  explicit ClientFilterableState(IsEnterpriseFunction is_enterprise_function);

  ClientFilterableState(const ClientFilterableState&) = delete;
  ClientFilterableState& operator=(const ClientFilterableState&) = delete;

  ~ClientFilterableState();

  // Whether this is an enterprise client. Always false on android, iOS, and
  // linux. Determined by VariationsServiceClient::IsEnterprise for windows,
  // chromeOs, and mac.
  bool IsEnterprise() const;

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

 private:
  // Evaluating enterprise status negatively affects performance, so we only
  // evaluate it if needed (i.e. if a study is filtering by enterprise) and at
  // most once.
  mutable IsEnterpriseFunction is_enterprise_function_;
  mutable absl::optional<bool> is_enterprise_;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_CLIENT_FILTERABLE_STATE_H_
