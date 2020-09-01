// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/user_policies_manager.h"

#include <limits>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/gcpw_strings.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/gaiacp/win_http_url_fetcher.h"

namespace credential_provider {
namespace {

// HTTP endpoint on the GCPW service to fetch user policies.
const char kUserIdUrlPlaceholder[] = "{user_id}";
const char kGcpwServiceFetchUserPoliciesPath[] = "/v1/users/{user_id}/policies";

// Default timeout when trying to make requests to the GCPW service.
const base::TimeDelta kDefaultFetchPoliciesRequestTimeout =
    base::TimeDelta::FromMilliseconds(5000);

// Path elements for the path where the policies are stored on disk.
constexpr base::FilePath::CharType kGcpwPoliciesDirectory[] = L"Policies";
constexpr base::FilePath::CharType kGcpwUserPolicyFileName[] =
    L"PolicyFetchResponse";

// Registry key where the the last time the policy is refreshed for the user is
// stored.
const wchar_t kLastUserPolicyRefreshTimeRegKey[] = L"last_policy_refresh_time";

// Maximum number of retries if a HTTP call to the backend fails.
constexpr unsigned int kMaxNumHttpRetries = 1;

// Registry key to control whether cloud policies feature is enabled.
const wchar_t kCloudPoliciesEnabledRegKey[] = L"cloud_policies_enabled";

// True when cloud policies feature is enabled.
bool g_cloud_policies_enabled = false;

// Get the path to the directory where the policies will be stored for the user
// with |sid|.
base::FilePath GetUserPolicyDirectoryFilePath(const base::string16& sid) {
  base::FilePath path = GetInstallDirectory();
  path = path.Append(kGcpwPoliciesDirectory).Append(sid);
  return path;
}

std::unique_ptr<base::File> GetOpenedPolicyFileForUser(
    const base::string16& sid,
    uint32_t open_flags) {
  base::FilePath policy_dir = GetUserPolicyDirectoryFilePath(sid);
  if (!base::DirectoryExists(policy_dir)) {
    base::File::Error error;
    if (!CreateDirectoryAndGetError(policy_dir, &error)) {
      LOGFN(ERROR) << "Policy data directory could not be created for " << sid
                   << " Error: " << error;
      return nullptr;
    }
  }

  base::FilePath policy_file_path = policy_dir.Append(kGcpwUserPolicyFileName);
  std::unique_ptr<base::File> policy_file(
      new base::File(policy_file_path, open_flags));

  if (!policy_file->IsValid()) {
    LOGFN(ERROR) << "Error opening policy file for user " << sid
                 << " with flags " << open_flags
                 << " Error: " << policy_file->error_details();
    return nullptr;
  }

  base::File::Error lock_error = policy_file->Lock();
  if (lock_error != base::File::FILE_OK) {
    LOGFN(ERROR) << "Failed to obtain exclusive lock on policy file! Error: "
                 << lock_error;
    return nullptr;
  }

  return policy_file;
}

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

UserPoliciesManager::UserPoliciesManager() : fetch_status_(S_OK) {
  g_cloud_policies_enabled =
      GetGlobalFlagOrDefault(kCloudPoliciesEnabledRegKey, 0) == 1;
}

UserPoliciesManager::~UserPoliciesManager() = default;

bool UserPoliciesManager::CloudPoliciesEnabled() const {
  return g_cloud_policies_enabled;
}

GURL UserPoliciesManager::GetGcpwServiceUserPoliciesUrl(
    const base::string16& sid) {
  GURL gcpw_service_url = GetGcpwServiceUrl();
  base::string16 user_id;

  HRESULT status = GetIdFromSid(sid.c_str(), &user_id);
  if (FAILED(status)) {
    LOGFN(ERROR) << "Could not get user id from sid " << sid;
    return GURL();
  }

  std::string user_policies_path(kGcpwServiceFetchUserPoliciesPath);
  std::string placeholder(kUserIdUrlPlaceholder);
  user_policies_path.replace(user_policies_path.find(placeholder),
                             placeholder.size(), base::UTF16ToUTF8(user_id));
  return gcpw_service_url.Resolve(user_policies_path);
}

HRESULT UserPoliciesManager::FetchAndStoreCloudUserPolicies(
    const base::string16& sid,
    const std::string& access_token) {
  fetch_status_ = E_FAIL;
  base::Optional<base::Value> request_result;

  GURL user_policies_url =
      UserPoliciesManager::Get()->GetGcpwServiceUserPoliciesUrl(sid);
  if (!user_policies_url.is_valid()) {
    return (fetch_status_ = E_FAIL);
  }

  // Make the fetch policies HTTP request.
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
                        base::File::FLAG_EXCLUSIVE_WRITE;
  std::unique_ptr<base::File> policy_file =
      GetOpenedPolicyFileForUser(sid, open_flags);
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
  base::string16 fetch_time_millis = base::NumberToString16(
      fetch_time.ToDeltaSinceWindowsEpoch().InMilliseconds());

  // Store the fetch time so we know whether a refresh is needed.
  SetUserProperty(sid, kLastUserPolicyRefreshTimeRegKey, fetch_time_millis);

  return (fetch_status_ = S_OK);
}

base::TimeDelta UserPoliciesManager::GetTimeDeltaSinceLastPolicyFetch(
    const base::string16& sid) const {
  wchar_t last_fetch_millis[512];
  ULONG last_fetch_size = base::size(last_fetch_millis);
  HRESULT hr = GetUserProperty(sid, kLastUserPolicyRefreshTimeRegKey,
                               last_fetch_millis, &last_fetch_size);

  if (FAILED(hr)) {
    // The policy was never fetched before.
    return base::TimeDelta::Max();
  }

  int64_t last_fetch_millis_int64;
  base::StringToInt64(last_fetch_millis, &last_fetch_millis_int64);

  int64_t time_delta_from_last_fetch_ms =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMilliseconds() -
      last_fetch_millis_int64;

  return base::TimeDelta::FromMilliseconds(time_delta_from_last_fetch_ms);
}

bool UserPoliciesManager::GetUserPolicies(const base::string16& sid,
                                          UserPolicies* user_policies) {
  DCHECK(user_policies);

  uint32_t open_flags = base::File::FLAG_OPEN | base::File::FLAG_READ;
  std::unique_ptr<base::File> policy_file =
      GetOpenedPolicyFileForUser(sid, open_flags);
  if (!policy_file) {
    return false;
  }

  std::vector<char> buffer(policy_file->GetLength());
  policy_file->Read(0, buffer.data(), buffer.size());
  policy_file.reset();

  base::Optional<base::Value> policy_data =
      base::JSONReader::Read(base::StringPiece(buffer.data(), buffer.size()),
                             base::JSON_ALLOW_TRAILING_COMMAS);
  if (!policy_data || !policy_data->is_dict()) {
    LOGFN(ERROR) << "Failed to read policy data from file!";
    return false;
  }

  // Override policies with those we just read.
  *user_policies = UserPolicies::FromValue(*policy_data);

  return true;
}

void UserPoliciesManager::SetCloudPoliciesEnabledForTesting(bool value) {
  g_cloud_policies_enabled = value;
}

HRESULT UserPoliciesManager::GetLastFetchStatusForTesting() const {
  return fetch_status_;
}

}  // namespace credential_provider
