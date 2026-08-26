// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/enterprise_management_metrics_provider.h"

#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "components/policy/core/common/enterprise_management_status_util.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/policy_service.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace policy {

namespace {

metrics::SystemProfileProto::EnterpriseManagement::PlatformStatus
ToProtoPlatformStatus(PlatformManagementStatus status) {
  switch (status) {
    case PlatformManagementStatus::kUnmanaged:
      return metrics::SystemProfileProto::EnterpriseManagement::
          PLATFORM_UNMANAGED;
    case PlatformManagementStatus::kComputerLocal:
      return metrics::SystemProfileProto::EnterpriseManagement::
          PLATFORM_COMPUTER_LOCAL;
    case PlatformManagementStatus::kDomainLocal:
      return metrics::SystemProfileProto::EnterpriseManagement::
          PLATFORM_DOMAIN_LOCAL;
    case PlatformManagementStatus::kCloud:
      return metrics::SystemProfileProto::EnterpriseManagement::PLATFORM_CLOUD;
    case PlatformManagementStatus::kCloudDomain:
      return metrics::SystemProfileProto::EnterpriseManagement::
          PLATFORM_CLOUD_DOMAIN;
  }
}

metrics::SystemProfileProto::EnterpriseManagement::BrowserStatus
ToProtoBrowserStatus(BrowserManagementStatus status) {
  switch (status) {
    case BrowserManagementStatus::kUnmanaged:
      return metrics::SystemProfileProto::EnterpriseManagement::
          BROWSER_UNMANAGED;
    case BrowserManagementStatus::kComputerLocalLe3:
      return metrics::SystemProfileProto::EnterpriseManagement::
          BROWSER_COMPUTER_LOCAL_LE3;
    case BrowserManagementStatus::kComputerLocalGt3:
      return metrics::SystemProfileProto::EnterpriseManagement::
          BROWSER_COMPUTER_LOCAL_GT3;
    case BrowserManagementStatus::kDomainLocal:
      return metrics::SystemProfileProto::EnterpriseManagement::
          BROWSER_DOMAIN_LOCAL;
    case BrowserManagementStatus::kCloud:
      return metrics::SystemProfileProto::EnterpriseManagement::BROWSER_CLOUD;
    case BrowserManagementStatus::kCloudDomain:
      return metrics::SystemProfileProto::EnterpriseManagement::
          BROWSER_CLOUD_DOMAIN;
  }
}

}  // namespace

EnterpriseManagementMetricsProvider::EnterpriseManagementMetricsProvider(
    ManagementService* platform_management_service,
    GetProfileStatesCallback get_profile_states_callback)
    : platform_management_service_(platform_management_service),
      get_profile_states_callback_(std::move(get_profile_states_callback)) {}

EnterpriseManagementMetricsProvider::~EnterpriseManagementMetricsProvider() =
    default;

void EnterpriseManagementMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  // 1. Record Platform metrics.
  for (PlatformManagementStatus status :
       GetPlatformManagementStatuses(platform_management_service_)) {
    base::UmaHistogramEnumeration(
        "Enterprise.ManagementService.PlatformManagementStatus", status);
  }

  // 2. Record Browser/Profile metrics.
  std::vector<ProfileState> states = get_profile_states_callback_.Run();
  for (const auto& state : states) {
    for (BrowserManagementStatus status : GetBrowserManagementStatuses(
             state.browser_management_service, state.policy_service)) {
      base::UmaHistogramEnumeration(
          "Enterprise.ManagementService.BrowserManagementStatus", status);
    }
  }
}

void EnterpriseManagementMetricsProvider::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  auto* enterprise = system_profile_proto->mutable_enterprise_management();

  // 1. Record Platform metrics.
  enterprise->clear_platform_status();
  for (PlatformManagementStatus status :
       GetPlatformManagementStatuses(platform_management_service_)) {
    enterprise->add_platform_status(ToProtoPlatformStatus(status));
  }

  // 2. Record Browser/Profile metrics across active profiles (de-duplicated).
  std::vector<ProfileState> states = get_profile_states_callback_.Run();
  base::flat_set<BrowserManagementStatus> unique_browser_statuses;
  for (const auto& state : states) {
    for (BrowserManagementStatus status : GetBrowserManagementStatuses(
             state.browser_management_service, state.policy_service)) {
      unique_browser_statuses.insert(status);
    }
  }

  enterprise->clear_browser_status();
  for (BrowserManagementStatus status : unique_browser_statuses) {
    enterprise->add_browser_status(ToProtoBrowserStatus(status));
  }
}

}  // namespace policy
