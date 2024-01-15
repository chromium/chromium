// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/experiments_fetcher.h"

#include <windows.h>

#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/common/chrome_version.h"
#include "chrome/credential_provider/gaiacp/experiments_manager.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"

namespace credential_provider {
namespace {

// HTTP endpoint on the GCPW service to fetch experiments.
const char16_t kGcpwServiceFetchExperimentsPath[] = u"/v1/experiments";

// Default timeout when trying to make requests to the GCPW service.
const base::TimeDelta kDefaultFetchExperimentsRequestTimeout =
    base::Milliseconds(5000);

// The period of refreshing experiments.
const base::TimeDelta kExperimentsRefreshExecutionPeriod = base::Hours(3);

// Maximum number of retries if a HTTP call to the backend fails.
constexpr unsigned int kMaxNumHttpRetries = 1;

// HTTP query parameters for fetch experiments RPC.
const char kObfuscatedUserIdKey[] = "obfuscated_gaia_id";
const char kGcpwVersionKey[] = "gcpw_version";
const char kDmTokenKey[] = "dm_token";
const char kDeviceResourceIdKey[] = "device_resource_id";
const char kFeaturesKey[] = "feature";

// Defines a task that is called by the ESA to perform the experiments fetch
// operation.
class ExperimentsFetchTask : public extension::Task {
 public:
  static std::unique_ptr<extension::Task> Create() {
    std::unique_ptr<extension::Task> esa_task(new ExperimentsFetchTask());
    return esa_task;
  }

  // ESA calls this to retrieve a configuration for the task execution. Return
  // a default config for now.
  extension::Config GetConfig() final {
    extension::Config config;
    config.execution_period = kExperimentsRefreshExecutionPeriod;
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
      HRESULT hr = ExperimentsFetcher::Get()->FetchAndStoreExperiments(c);
      if (FAILED(hr)) {
        LOGFN(ERROR) << "Failed fetching experiments for " << c.user_sid
                     << ". hr=" << putHR(hr);
        task_status = hr;
      }
    }

    return task_status;
  }

 private:
  std::vector<extension::UserDeviceContext> context_;
};

// Builds the request dictionary to fetch experiments from the backend. If
// |dm_token| is empty, it isn't added into request. If user id isn't found for
// the given |sid|, returns an empty dictionary.
std::unique_ptr<base::Value::Dict> GetExperimentsRequestDict(
    const std::wstring& sid,
    const std::wstring& device_resource_id,
    const std::wstring& dm_token) {
  std::unique_ptr<base::Value::Dict> dict(new base::Value::Dict);

  std::wstring user_id;

  HRESULT status = GetIdFromSid(sid.c_str(), &user_id);
  if (FAILED(status)) {
    LOGFN(ERROR) << "Could not get user id from sid " << sid;
    return nullptr;
  }

  dict->Set(kObfuscatedUserIdKey, base::WideToUTF8(user_id));

  if (!dm_token.empty()) {
    dict->Set(kDmTokenKey, base::WideToUTF8(dm_token));
  }
  dict->Set(kDeviceResourceIdKey, base::WideToUTF8(device_resource_id));

  dict->Set(kGcpwVersionKey, base::WideToUTF8(TEXT(CHROME_VERSION_STRING)));

  base::Value::List keys;
  for (auto& experiment : ExperimentsManager::Get()->GetExperimentsList())
    keys.Append(experiment);

  dict->Set(kFeaturesKey, std::move(keys));

  return dict;
}

}  // namespace

GURL ExperimentsFetcher::GetExperimentsUrl() {
  GURL gcpw_service_url = GetGcpwServiceUrl();
  return gcpw_service_url.Resolve(kGcpwServiceFetchExperimentsPath);
}

// static
ExperimentsFetcher* ExperimentsFetcher::Get() {
  return *GetInstanceStorage();
}

// static
ExperimentsFetcher** ExperimentsFetcher::GetInstanceStorage() {
  static ExperimentsFetcher instance;
  static ExperimentsFetcher* instance_storage = &instance;
  return &instance_storage;
}

// static
extension::TaskCreator ExperimentsFetcher::GetFetchExperimentsTaskCreator() {
  return base::BindRepeating(&ExperimentsFetchTask::Create);
}

ExperimentsFetcher::ExperimentsFetcher() {}

ExperimentsFetcher::~ExperimentsFetcher() = default;

HRESULT ExperimentsFetcher::FetchAndStoreExperiments(
    const extension::UserDeviceContext& context) {
  return FetchAndStoreExperimentsInternal(
      context.user_sid, /* access_token= */ "",
      GetExperimentsRequestDict(context.user_sid, context.device_resource_id,
                                context.dm_token));
}

HRESULT ExperimentsFetcher::FetchAndStoreExperiments(
    const std::wstring& sid,
    const std::string& access_token) {
  HRESULT hr = FetchAndStoreExperimentsInternal(
      sid, access_token,
      GetExperimentsRequestDict(sid, GetUserDeviceResourceId(sid),
                                /* dm_token= */ L""));
  return hr;
}

HRESULT ExperimentsFetcher::FetchAndStoreExperimentsInternal(
    const std::wstring& sid,
    const std::string& access_token,
    std::unique_ptr<base::Value::Dict> request_dict) {
  if (!request_dict) {
    LOGFN(ERROR) << "Request dictionary is null";
    return E_FAIL;
  }

  // Make the fetch experiments HTTP request.
  std::optional<base::Value> request_result;
  HRESULT hr = WinHttpUrlFetcher::BuildRequestAndFetchResultFromHttpService(
      GetExperimentsUrl(), access_token, {}, *request_dict,
      kDefaultFetchExperimentsRequestTimeout, kMaxNumHttpRetries,
      &request_result);

  if (FAILED(hr)) {
    LOGFN(ERROR) << "BuildRequestAndFetchResultFromHttpService hr="
                 << putHR(hr);
    return hr;
  }

  std::string experiments_data;
  if (request_result && request_result->is_dict()) {
    if (!base::JSONWriter::Write(*request_result, &experiments_data)) {
      LOGFN(ERROR) << "base::JSONWriter::Write failed";
      return E_FAIL;
    }
  } else {
    LOGFN(ERROR) << "Failed to parse experiments response!";
    return E_FAIL;
  }

  uint32_t open_flags = base::File::FLAG_CREATE_ALWAYS |
                        base::File::FLAG_WRITE |
                        base::File::FLAG_WIN_EXCLUSIVE_WRITE;
  std::unique_ptr<base::File> experiments_file = GetOpenedFileForUser(
      sid, open_flags, kGcpwExperimentsDirectory, kGcpwUserExperimentsFileName);
  if (!experiments_file) {
    LOGFN(ERROR) << "Failed to open " << kGcpwUserExperimentsFileName
                 << " file.";
    return E_FAIL;
  }

  int num_bytes_written = experiments_file->Write(0, experiments_data.c_str(),
                                                  experiments_data.size());

  experiments_file.reset();

  if (size_t(num_bytes_written) != experiments_data.size()) {
    LOGFN(ERROR) << "Failed writing experiments data to file! Only "
                 << num_bytes_written << " bytes written out of "
                 << experiments_data.size();
    return E_FAIL;
  }

  base::Time fetch_time = base::Time::Now();
  std::wstring fetch_time_millis = base::NumberToWString(
      fetch_time.ToDeltaSinceWindowsEpoch().InMilliseconds());

  if (!ExperimentsManager::Get()->ReloadExperiments(sid)) {
    LOGFN(ERROR) << "Error when loading experiments for user with sid " << sid;
  }

  // Store the fetch time so we know whether a refresh is needed.
  SetUserProperty(sid, kLastUserExperimentsRefreshTimeRegKey,
                  fetch_time_millis);

  return S_OK;
}

}  // namespace credential_provider
