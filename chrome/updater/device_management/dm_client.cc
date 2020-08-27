// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/device_management/dm_client.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/device_management/dm_cached_policy_info.h"
#include "chrome/updater/device_management/dm_storage.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/update_client/network.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "chrome/updater/win/net/network.h"
#elif defined(OS_MAC)
#include "chrome/updater/mac/net/network.h"
#endif

namespace updater {

namespace {

// Content-type of DM requests.
constexpr char kDMContentType[] = "application/x-protobuf";

// String constants for the device and app type we report to the server.
constexpr char kParamAgent[] = "agent";
constexpr char kParamRequest[] = "request";
constexpr char kParamPlatform[] = "platform";
constexpr char kParamDeviceID[] = "deviceid";
constexpr char kParamAppType[] = "apptype";
constexpr char kValueAppType[] = "Chrome";

constexpr char kAuthorizationHeader[] = "Authorization";

// Constants for device management enrollment requests.
constexpr char kRegistrationRequestType[] = "register_policy_agent";
constexpr char kRegistrationTokenType[] = "GoogleEnrollmentToken";

// Constants for policy fetch requests.
constexpr char kPolicyFetchRequestType[] = "policy";
constexpr char kPolicyFetchTokenType[] = "GoogleDMToken";
constexpr char kGoogleUpdateMachineLevelApps[] = "google/machine-level-apps";

constexpr int kHTTPStatusOK = 200;
constexpr int kHTTPStatusGone = 410;

class DefaultConfigurator : public DMClient::Configurator {
 public:
  DefaultConfigurator();
  ~DefaultConfigurator() override = default;

  std::string GetDMServerUrl() const override {
    return kDeviceManagementServerURL;
  }

  std::string GetAgentParameter() const override {
    return "Updater-" UPDATER_VERSION_STRING;
  }

  std::string GetPlatformParameter() const override;

  std::unique_ptr<update_client::NetworkFetcher> CreateNetworkFetcher()
      const override {
    return network_fetcher_factory_->Create();
  }

 private:
  scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory_;
};

DefaultConfigurator::DefaultConfigurator()
    : network_fetcher_factory_(base::MakeRefCounted<NetworkFetcherFactory>()) {}

std::string DefaultConfigurator::GetPlatformParameter() const {
  std::string os_name = base::SysInfo::OperatingSystemName();
  std::string os_hardware = base::SysInfo::OperatingSystemArchitecture();
  int32_t os_major_version = 0;
  int32_t os_minor_version = 0;
  int32_t os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(
      &os_major_version, &os_minor_version, &os_bugfix_version);
  std::string os_version = base::StringPrintf(
      "%d.%d.%d", os_major_version, os_minor_version, os_bugfix_version);

  return base::StringPrintf("%s|%s|%s", os_name.c_str(), os_hardware.c_str(),
                            os_version.c_str());
}

}  // namespace

DMClient::DMClient()
    : DMClient(std::make_unique<DefaultConfigurator>(), GetDefaultDMStorage()) {
}

DMClient::DMClient(std::unique_ptr<Configurator> config,
                   scoped_refptr<DMStorage> storage)
    : config_(std::move(config)),
      storage_(std::move(storage)),
      http_status_code_(-1) {}

DMClient::~DMClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

GURL DMClient::BuildURL(const std::string& request_type) const {
  GURL url(config_->GetDMServerUrl());
  url = AppendQueryParameter(url, kParamRequest, request_type);
  url = AppendQueryParameter(url, kParamAppType, kValueAppType);
  url = AppendQueryParameter(url, kParamAgent, config_->GetAgentParameter());
  url = AppendQueryParameter(url, kParamPlatform,
                             config_->GetPlatformParameter());
  return AppendQueryParameter(url, kParamDeviceID, storage_->GetDeviceID());
}

scoped_refptr<DMStorage> DMClient::GetStorage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_;
}

void DMClient::PostRegisterRequest(DMRequestCallback request_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RequestResult result = RequestResult::kSuccess;
  const std::string enrollment_token = storage_->GetEnrollmentToken();
  network_fetcher_ = config_->CreateNetworkFetcher();
  request_callback_ = std::move(request_callback);

  if (storage_->IsDeviceDeregistered()) {
    result = RequestResult::kDeregistered;
  } else if (!storage_->GetDmToken().empty()) {
    result = RequestResult::kAleadyRegistered;
  } else if (enrollment_token.empty()) {
    result = RequestResult::kNotManaged;
  } else if (storage_->GetDeviceID().empty()) {
    result = RequestResult::kNoDeviceID;
  } else if (!network_fetcher_) {
    result = RequestResult::kFetcherError;
  }

  if (result != RequestResult::kSuccess) {
    VLOG(1) << "Device registration skipped with DM error: "
            << static_cast<int>(result);
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(request_callback_), result));
    return;
  }

  const std::string data = GetRegisterBrowserRequestData(
      policy::GetMachineName(), policy::GetOSPlatform(),
      policy::GetOSVersion());

  // Authorization token is the enrollment token for device registration.
  const base::flat_map<std::string, std::string> additional_headers = {
      {kAuthorizationHeader,
       base::StringPrintf("%s token=%s", kRegistrationTokenType,
                          enrollment_token.c_str())},
  };

  network_fetcher_->PostRequest(
      BuildURL(kRegistrationRequestType), data, kDMContentType,
      additional_headers,
      base::BindOnce(&DMClient::OnRequestStarted, base::Unretained(this)),
      base::BindRepeating(&DMClient::OnRequestProgress, base::Unretained(this)),
      base::BindOnce(&DMClient::OnRegisterRequestComplete,
                     base::Unretained(this)));
}

void DMClient::OnRequestStarted(int response_code, int64_t content_length) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "POST request is sent to DM server, status: " << response_code
          << ". Content length: " << content_length << ".";
  http_status_code_ = response_code;
}

void DMClient::OnRequestProgress(int64_t current) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "POST request progess made, current bytes: " << current;
}

void DMClient::OnRegisterRequestComplete(
    std::unique_ptr<std::string> response_body,
    int net_error,
    const std::string& header_etag,
    const std::string& header_x_cup_server_proof,
    int64_t xheader_retry_after_sec) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RequestResult request_result = RequestResult::kSuccess;

  if (net_error != 0) {
    VLOG(1) << "DM register failed due to net error: " << net_error;
    request_result = RequestResult::kNetworkError;
  } else if (http_status_code_ == kHTTPStatusGone) {
    VLOG(1) << "Device is now de-registered.";
    storage_->DeregisterDevice();
  } else if (http_status_code_ != kHTTPStatusOK) {
    VLOG(1) << "DM device registration failed due to http error: "
            << http_status_code_;
    request_result = RequestResult::kHttpError;
  } else {
    const std::string dm_token =
        ParseDeviceRegistrationResponse(*response_body);
    if (dm_token.empty()) {
      VLOG(1) << "Failed to parse DM token from registration response.";
      request_result = RequestResult::kUnexpectedResponse;
    } else {
      VLOG(1) << "Register request completed, got DM token: " << dm_token;
      if (!storage_->StoreDmToken(dm_token))
        request_result = RequestResult::kSerializationError;
    }
  }

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(request_callback_), request_result));
}

void DMClient::PostPolicyFetchRequest(DMRequestCallback request_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RequestResult result = RequestResult::kSuccess;
  const std::string dm_token = storage_->GetDmToken();
  network_fetcher_ = config_->CreateNetworkFetcher();
  request_callback_ = std::move(request_callback);

  if (storage_->IsDeviceDeregistered()) {
    result = RequestResult::kDeregistered;
  } else if (dm_token.empty()) {
    result = RequestResult::kNoDMToken;
  } else if (storage_->GetDeviceID().empty()) {
    result = RequestResult::kNoDeviceID;
  } else if (!network_fetcher_) {
    result = RequestResult::kFetcherError;
  }

  if (result != RequestResult::kSuccess) {
    VLOG(1) << "Policy fetch skipped with DM error: "
            << static_cast<int>(result);
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(request_callback_), result));
    return;
  }

  cached_info_ = storage_->GetCachedPolicyInfo();
  const std::string data =
      GetPolicyFetchRequestData(kGoogleUpdateMachineLevelApps, *cached_info_);

  // Authorization token is the DM token for policy fetch
  const base::flat_map<std::string, std::string> additional_headers = {
      {kAuthorizationHeader,
       base::StringPrintf("%s token=%s", kPolicyFetchTokenType,
                          dm_token.c_str())},
  };

  network_fetcher_->PostRequest(
      BuildURL(kPolicyFetchRequestType), data, kDMContentType,
      additional_headers,
      base::BindOnce(&DMClient::OnRequestStarted, base::Unretained(this)),
      base::BindRepeating(&DMClient::OnRequestProgress, base::Unretained(this)),
      base::BindOnce(&DMClient::OnPolicyFetchRequestComplete,
                     base::Unretained(this)));
}

void DMClient::OnPolicyFetchRequestComplete(
    std::unique_ptr<std::string> response_body,
    int net_error,
    const std::string& header_etag,
    const std::string& header_x_cup_server_proof,
    int64_t xheader_retry_after_sec) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RequestResult request_result = RequestResult::kSuccess;

  if (net_error != 0) {
    VLOG(1) << "DM policy fetch failed due to net error: " << net_error;
    request_result = RequestResult::kNetworkError;
  } else if (http_status_code_ == kHTTPStatusGone) {
    VLOG(1) << "Device is now de-registered.";
    storage_->DeregisterDevice();
  } else if (http_status_code_ != kHTTPStatusOK) {
    VLOG(1) << "DM policy fetch failed due to http error: "
            << http_status_code_;
    request_result = RequestResult::kHttpError;
  } else {
    DMPolicyMap policies = ParsePolicyFetchResponse(
        *response_body, *cached_info_, storage_->GetDmToken(),
        storage_->GetDeviceID());

    if (policies.empty()) {
      request_result = RequestResult::kUnexpectedResponse;
    } else {
      VLOG(1) << "Policy fetch request completed, got " << policies.size()
              << " new policies.";
      if (!storage_->PersistPolicies(policies))
        request_result = RequestResult::kSerializationError;
    }
  }

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(request_callback_), request_result));
}

}  // namespace updater
