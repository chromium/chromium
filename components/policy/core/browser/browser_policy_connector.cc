// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/browser_policy_connector.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_statistics_collector.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/icu/source/i18n/unicode/regex.h"

namespace policy {

namespace {

// The URL for the device management server.
const char kDefaultDeviceManagementServerUrl[] =
    "https://m.google.com/devicemanagement/data/api";

const char kDefaultEncryptedReportingServerUrl[] =
    "https://chromereporting-pa.googleapis.com/v1/record";

// The URL for the realtime reporting server.
const char kDefaultRealtimeReportingServerUrl[] =
    "https://chromereporting-pa.googleapis.com/v1/events";

// Regexes that match many of the larger public email providers as we know
// these users are not from hosted enterprise domains.
const wchar_t* const kNonManagedDomainPatterns[] = {
  L"aol\\.com",
  L"comcast\\.net",
  L"googlemail\\.com",
  L"gmail\\.com",
  L"gmx\\.de",
  L"hotmail(\\.co|\\.com|)\\.[^.]+",  // hotmail.com, hotmail.it, hotmail.co.uk
  L"live\\.com",
  L"mail\\.ru",
  L"msn\\.com",
  L"naver\\.com",
  L"orange\\.fr",
  L"outlook\\.com",
  L"qq\\.com",
  L"yahoo(\\.co|\\.com|)\\.[^.]+",  // yahoo.com, yahoo.co.uk, yahoo.com.tw
  L"yandex\\.ru",
  L"web\\.de",
  L"wp\\.pl",
  L"consumer\\.example\\.com",
};

const char* non_managed_domain_for_testing = nullptr;

// Returns true if |domain| matches the regex |pattern|.
bool MatchDomain(const std::u16string& domain,
                 const std::u16string& pattern,
                 size_t index) {
  UErrorCode status = U_ZERO_ERROR;
  const icu::UnicodeString icu_pattern(pattern.data(), pattern.length());
  icu::RegexMatcher matcher(icu_pattern, UREGEX_CASE_INSENSITIVE, status);
  if (!U_SUCCESS(status)) {
    // http://crbug.com/365351 - if for some reason the matcher creation fails
    // just return that the pattern doesn't match the domain. This is safe
    // because the calling method (IsNonEnterpriseUser()) is just used to enable
    // an optimization for non-enterprise users - better to skip the
    // optimization than crash.
    DLOG(ERROR) << "Possible invalid domain pattern: " << pattern
                << " - Error: " << status;
    return false;
  }
  icu::UnicodeString icu_input(domain.data(), domain.length());
  matcher.reset(icu_input);
  status = U_ZERO_ERROR;
  UBool match = matcher.matches(status);
  DCHECK(U_SUCCESS(status));
  return !!match;  // !! == convert from UBool to bool.
}

}  // namespace

BrowserPolicyConnector::BrowserPolicyConnector(
    const HandlerListFactory& handler_list_factory)
    : BrowserPolicyConnectorBase(handler_list_factory) {
}

BrowserPolicyConnector::~BrowserPolicyConnector() {
}

void BrowserPolicyConnector::InitInternal(
    PrefService* local_state,
    std::unique_ptr<DeviceManagementService> device_management_service) {
  device_management_service_ = std::move(device_management_service);

  policy_statistics_collector_ =
      std::make_unique<policy::PolicyStatisticsCollector>(
          base::BindRepeating(&GetChromePolicyDetails), GetChromeSchema(),
          GetPolicyService(), local_state, base::ThreadTaskRunnerHandle::Get());
  policy_statistics_collector_->Initialize();
}

void BrowserPolicyConnector::Shutdown() {
  BrowserPolicyConnectorBase::Shutdown();
  device_management_service_.reset();
}

void BrowserPolicyConnector::ScheduleServiceInitialization(
    int64_t delay_milliseconds) {
  // Skip device initialization if the BrowserPolicyConnector was never
  // initialized (unit tests).
  if (device_management_service_)
    device_management_service_->ScheduleInitialization(delay_milliseconds);
  else
    CHECK_IS_TEST();
}

bool BrowserPolicyConnector::ProviderHasPolicies(
    const ConfigurationPolicyProvider* provider) const {
  if (!provider)
    return false;
  for (const auto& pair : provider->policies()) {
    if (!pair.second.empty())
      return true;
  }
  return false;
}

std::string BrowserPolicyConnector::GetDeviceManagementUrl() const {
  return GetUrlOverride(switches::kDeviceManagementUrl,
                        kDefaultDeviceManagementServerUrl);
}

std::string BrowserPolicyConnector::GetRealtimeReportingUrl() const {
  return GetUrlOverride(switches::kRealtimeReportingUrl,
                        kDefaultRealtimeReportingServerUrl);
}

std::string BrowserPolicyConnector::GetEncryptedReportingUrl() const {
  return GetUrlOverride(switches::kEncryptedReportingUrl,
                        kDefaultEncryptedReportingServerUrl);
}

std::string BrowserPolicyConnector::GetUrlOverride(
    const char* flag,
    const char* default_value) const {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(flag)) {
    if (IsCommandLineSwitchSupported())
      return command_line->GetSwitchValueASCII(flag);
    else
      LOG(WARNING) << flag << " not supported on this channel";
  }
  return default_value;
}

// static
bool BrowserPolicyConnector::IsNonEnterpriseUser(const std::string& username) {
  TRACE_EVENT0("browser", "BrowserPolicyConnector::IsNonEnterpriseUser");
  if (username.empty() || username.find('@') == std::string::npos) {
    // An empty username means incognito user in case of ChromiumOS and
    // no logged-in user in case of Chromium (SigninService). Many tests use
    // nonsense email addresses (e.g. 'test') so treat those as non-enterprise
    // users.
    return true;
  }
  const std::u16string domain = base::UTF8ToUTF16(
      gaia::ExtractDomainName(gaia::CanonicalizeEmail(username)));
  for (size_t i = 0; i < std::size(kNonManagedDomainPatterns); i++) {
    std::u16string pattern = base::WideToUTF16(kNonManagedDomainPatterns[i]);
    if (MatchDomain(domain, pattern, i))
      return true;
  }
  if (non_managed_domain_for_testing &&
      domain == base::UTF8ToUTF16(non_managed_domain_for_testing)) {
    return true;
  }
  return false;
}

// static
void BrowserPolicyConnector::SetNonEnterpriseDomainForTesting(
    const char* domain) {
  non_managed_domain_for_testing = domain;
}

// static
void BrowserPolicyConnector::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      policy_prefs::kUserPolicyRefreshRate,
      CloudPolicyRefreshScheduler::kDefaultRefreshDelayMs);
  registry->RegisterBooleanPref(
      policy_prefs::kCloudManagementEnrollmentMandatory, false);
}

}  // namespace policy
