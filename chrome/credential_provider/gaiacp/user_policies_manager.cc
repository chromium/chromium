// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/user_policies_manager.h"

#include <limits>
#include <string_view>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/gcpw_strings.h"
#include "chrome/credential_provider/gaiacp/internet_availability_checker.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/os_user_manager.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/gaiacp/win_http_url_fetcher.h"

namespace credential_provider {
namespace {

// HTTP endpoint on the GCPW service to fetch user policies.
const char kUserIdUrlPlaceholder[] = "{user_id}";
const char kGcpwServiceFetchUserPoliciesPath[] = "/v1/users/{user_id}/policies";
const char kGcpwServiceFetchUserPoliciesQueryTemplate[] =
    "?device_resource_id=%s&dm_token=%s";

// Default timeout when trying to make requests to the GCPW service.
const base::TimeDelta kDefaultFetchPoliciesRequestTimeout =
    base::Milliseconds(5000);

// Path elements for the path where the policies are stored on disk.
constexpr wchar_t kGcpwPoliciesDirectory[] = L"Policies";
constexpr wchar_t kGcpwUserPolicyFileName[] = L"PolicyFetchResponse";

// Maximum number of retries if a HTTP call to the backend fails.
constexpr unsigned int kMaxNumHttpRetries = 1;

// Registry key to control whether cloud policies feature is enabled.
const wchar_t kCloudPoliciesEnabledRegKey[] = L"cloud_policies_enabled";

// Name of the key in the server response whose value contains the user
// policies.
const char kPolicyFetchResponseKeyName[] = "policies";

// The period of refreshing cloud policies.
const base::TimeDelta kCloudPoliciesExecutionPeriod = base::Hours(1);

// True when cloud policies feature is enabled.
bool g_cloud_policies_enabled = false;

// Creates the URL used to fetch the policies from the backend based on the
// credential present (OAuth vs DM token) for authentication.
GURL GetFetchUserPoliciesUrl(const std::wstring& sid,
                             bool has_access_token,
                             const std::wstring& device_resource_id,
                             const std::wstring& dm_token) {
  GURL gcpw_service_url = GetGcpwServiceUrl();
  std::wstring user_id;

  HRESULT status = GetIdFromSid(sid.c_str(), &user_id);
  if (FAILED(status)) {
    LOGFN(ERROR) << "Could not get user id from sid " << sid;
    return GURL();
  }

  std::string user_policies_path(kGcpwServiceFetchUserPoliciesPath);
  std::string placeholder(kUserIdUrlPlaceholder);
  user_policies_path.replace(user_policies_path.find(placeholder),
                             placeholder.size(), base::WideToUTF8(user_id));

  if (!has_access_token) {
    if (device_resource_id.empty() || dm_token.empty()) {
      LOGFN(ERROR) << "Either device id or dm token empty when no access token "
                      "present for "
                   << sid;
      return GURL();
    }

    std::string device_resource_id_value = base::WideToUTF8(device_resource_id);
    std::string dm_token_value = base::WideToUTF8(dm_token);
    std::string query_suffix = base::StringPrintf(
        kGcpwServiceFetchUserPoliciesQueryTemplate,
        device_resource_id_value.c_str(), dm_token_value.c_str());
    user_policies_path += query_suffix;
  }

  return gcpw_service_url.Resolve(user_policies_path);
}

// Defines a task that is called by the ESA to perform the policy fetch
// operation.
class UserPoliciesFetchTask : public extension::Task {
 public:
  static std::unique_ptr<extension::Task> Create() {
    std::unique_ptr<extension::Task> esa_task(new UserPoliciesFetchTask());
    return esa_task;
  }

  // ESA calls this to retrieve a configuration for the task execution. Return
  // the 1 hour period for the user policies fetch.
  extension::Config GetConfig() final {
    extension::Config config;
    config.execution_period = kCloudPoliciesExecutionPeriod;
    return config;
  }

  // ESA calls this to set all the user-device contexts for the execution of the
  // task.
  HRESULT SetContext(const std::vector<extension::UserDeviceContext>& c) final {
    context_ = c;
    return S_OK;
  }

  // ESA calls execute function to perform the actual task.
  HRESULT Execute() final {
    HRESULT task_status = S_OK;
    for (const auto& c : context_) {
      HRESULT hr =
          UserPoliciesManager::Get()->FetchAndStoreCloudUserPolicies(c);
      if (FAILED(hr)) {
        LOGFN(ERROR) << "Failed fetching policies for " << c.user_sid
                     << ". hr=" << putHR(hr);
        task_status = hr;
      }
    }
    return task_status;
  }

 private:
  std::vector<extension::UserDeviceContext> context_;
};

}  // namespace

// static
UserPoliciesManager* UserPoliciesManager::Get() {
  return *GetInstanceStorage();
}

// static
UserPoliciesManager** UserPoliciesManager::GetInstanceStorage() {
  static UserPoliciesManager instance;
  static UserPoliciesManager* instance_storage = &instance;
  return &instance_storage;
}

// static
extension::TaskCreator UserPoliciesManager::GetFetchPoliciesTaskCreator() {
  return base::BindRepeating(&UserPoliciesFetchTask::Create);
}

UserPoliciesManager::UserPoliciesManager() : fetch_status_(S_OK) {
  g_cloud_policies_enabled =
      GetGlobalFlagOrDefault(kCloudPoliciesEnabledRegKey, 1) == 1;
}

UserPoliciesManager::~UserPoliciesManager() = default;

bool UserPoliciesManager::CloudPoliciesEnabled() const {
  return g_cloud_policies_enabled;
}

GURL UserPoliciesManager::GetGcpwServiceUserPoliciesUrl(
    const std::wstring& sid) {
  return GetFetchUserPoliciesUrl(sid, true, L"", L"");
}

GURL UserPoliciesManager::GetGcpwServiceUserPoliciesUrl(
    const std::wstring& sid,
    const std::wstring& device_resource_id,
    const std::wstring& dm_token) {
  return GetFetchUserPoliciesUrl(sid, false, device_resource_id, dm_token);
}

HRESULT UserPoliciesManager::FetchAndStoreCloudUserPolicies(
    const extension::UserDeviceContext& context) {
  return FetchAndStorePolicies(
      context.user_sid,
      GetGcpwServiceUserPoliciesUrl(
          context.user_sid, context.device_resource_id, context.dm_token),
      std::string());
}

HRESULT UserPoliciesManager::FetchAndStoreCloudUserPolicies(
    const std::wstring& sid,
    const std::string& access_token) {
  if (access_token.empty()) {
    LOGFN(ERROR) << "Access token not specified";
    return (fetch_status_ = E_FAIL);
  }

  return FetchAndStorePolicies(sid, GetGcpwServiceUserPoliciesUrl(sid),
                               access_token);
}

HRESULT UserPoliciesManager::FetchAndStorePolicies(
    const std::wstring& sid,
    GURL user_policies_url,
    const std::string& access_token) {
  fetch_status_ = E_FAIL;

  if (!user_policies_url.is_valid()) {
    LOGFN(ERROR) << "Invalid user policies fetch URL specified.";
    return (fetch_status_ = E_FAIL);
  }

  // Make the fetch policies HTTP request.
  std::optional<base::Value> request_result;
  HRESULT hr = WinHttpUrlFetcher::BuildRequestAndFetchResultFromHttpService(
      user_policies_url, access_token, {}, {},
      kDefaultFetchPoliciesRequestTimeout, kMaxNumHttpRetries, &request_result);

  if (FAILED(hr)) {
    LOGFN(ERROR) << "BuildRequestAndFetchResultFromHttpService hr="
                 << putHR(hr);
    return (fetch_status_ = hr);
  }

  std::string policy_data;
  if (request_result && request_result->is_dict()) {
    if (!base::JSONWriter::Write(*request_result, &policy_data)) {
      LOGFN(ERROR) << "base::JSONWriter::Write failed";
      return (fetch_status_ = E_FAIL);
    }
  } else {
    LOGFN(ERROR) << "Failed to parse policy response!";
    return (fetch_status_ = E_FAIL);
  }

  uint32_t open_flags = base::File::FLAG_CREATE_ALWAYS |
                        base::File::FLAG_WRITE |
                        base::File::FLAG_WIN_EXCLUSIVE_WRITE;
  std::unique_ptr<base::File> policy_file = GetOpenedFileForUser(
      sid, open_flags, kGcpwPoliciesDirectory, kGcpwUserPolicyFileName);
  if (!policy_file) {
    return (fetch_status_ = E_FAIL);
  }

  int num_bytes_written =
      policy_file->Write(0, policy_data.c_str(), policy_data.size());

  policy_file.reset();

  if (size_t(num_bytes_written) != policy_data.size()) {
    LOGFN(ERROR) << "Failed writing policy data to file! Only "
                 << num_bytes_written << " bytes written out of "
                 << policy_data.size();
    return (fetch_status_ = E_FAIL);
  }

  base::Time fetch_time = base::Time::Now();
  std::wstring fetch_time_millis = base::NumberToWString(
      fetch_time.ToDeltaSinceWindowsEpoch().InMilliseconds());

  // Store the fetch time so we know whether a refresh is needed.
  SetUserProperty(sid, kLastUserPolicyRefreshTimeRegKey, fetch_time_millis);

  return (fetch_status_ = S_OK);
}

bool UserPoliciesManager::GetUserPolicies(const std::wstring& sid,
                                          UserPolicies* user_policies) const {
  DCHECK(user_policies);

  uint32_t open_flags = base::File::FLAG_OPEN | base::File::FLAG_READ;
  std::unique_ptr<base::File> policy_file = GetOpenedFileForUser(
      sid, open_flags, kGcpwPoliciesDirectory, kGcpwUserPolicyFileName);
  if (!policy_file) {
    return false;
  }

  std::vector<char> buffer(policy_file->GetLength());
  policy_file->Read(0, buffer.data(), buffer.size());
  policy_file.reset();

  std::optional<base::Value> policy_data =
      base::JSONReader::Read(std::string_view(buffer.data(), buffer.size()),
                             base::JSON_ALLOW_TRAILING_COMMAS);
  if (!policy_data || !policy_data->is_dict()) {
    LOGFN(ERROR) << "Failed to read policy data from file!";
    return false;
  }

  const base::Value::Dict* policies =
      policy_data->GetDict().FindDict(kPolicyFetchResponseKeyName);
  if (!policies) {
    LOGFN(ERROR) << "User policies not found!";
    return false;
  }

  // Override policies with those we just read.
  *user_policies = UserPolicies::FromValue(*policies);

  return true;
}

bool UserPoliciesManager::IsUserPolicyStaleOrMissing(
    const std::wstring& sid) const {
  UserPolicies user_policies;
  if (!GetUserPolicies(sid, &user_policies)) {
    LOGFN(VERBOSE) << "User policy file doesn't exist";
    return true;
  }

  // if the policy file exists but is stale, check the internet connection and
  // try to fetch the new policy file. If there's no internet connection, will
  // return false and GCPW will continue to use the stale policy information.
  if (GetTimeDeltaSinceLastFetch(sid, kLastUserPolicyRefreshTimeRegKey) >
      kMaxTimeDeltaSinceLastUserPolicyRefresh) {
    if (!InternetAvailabilityChecker::Get()->HasInternetConnection()) {
      LOGFN(VERBOSE)
          << "There's no internet connection to update stale policy file.";
      return false;
    }

    LOGFN(VERBOSE) << "User policy file is stale";
    return true;
  }

  return false;
}

void UserPoliciesManager::SetCloudPoliciesEnabledForTesting(bool value) {
  g_cloud_policies_enabled = value;
}

HRESULT UserPoliciesManager::GetLastFetchStatusForTesting() const {
  return fetch_status_;
}

void UserPoliciesManager::SetFakesForTesting(FakesForTesting* fakes) {
  DCHECK(fakes);

  WinHttpUrlFetcher::SetCreatorForTesting(
      fakes->fake_win_http_url_fetcher_creator);
  if (fakes->os_user_manager_for_testing) {
    OSUserManager::SetInstanceForTesting(fakes->os_user_manager_for_testing);
  }
  if (fakes->internet_availability_checker_for_testing) {
    InternetAvailabilityChecker::SetInstanceForTesting(
        fakes->internet_availability_checker_for_testing);
  }
}

}  // namespace credential_provider
