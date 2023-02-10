// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/device_management/dm_client.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/updater/device_management/dm_cached_policy_info.h"
#include "chrome/updater/device_management/dm_response_validator.h"
#include "chrome/updater/device_management/dm_storage.h"
#include "chrome/updater/net/network.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/util.h"
#include "components/update_client/network.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

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
constexpr char kDMTokenType[] = "GoogleDMToken";
constexpr char kGoogleUpdateMachineLevelApps[] = "google/machine-level-apps";

// Constants for policy validation report requests.
constexpr char kValidationReportRequestType[] = "policy_validation_report";

constexpr int kHTTPStatusOK = 200;
constexpr int kHTTPStatusGone = 410;

class DefaultConfigurator : public DMClient::Configurator {
 public:
  explicit DefaultConfigurator(absl::optional<PolicyServiceProxyConfiguration>
                                   policy_service_proxy_configuration);
  ~DefaultConfigurator() override = default;

  std::string GetDMServerUrl() const override {
    return DEVICE_MANAGEMENT_SERVER_URL;
  }

  std::string GetAgentParameter() const override {
    return base::StrCat({"Updater-", kUpdaterVersion});
  }

  std::string GetPlatformParameter() const override;

  std::unique_ptr<update_client::NetworkFetcher> CreateNetworkFetcher()
      const override {
    return network_fetcher_factory_->Create();
  }

 private:
  scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory_;
};

DefaultConfigurator::DefaultConfigurator(
    absl::optional<PolicyServiceProxyConfiguration>
        policy_service_proxy_configuration)
    : network_fetcher_factory_(base::MakeRefCounted<NetworkFetcherFactory>(
          policy_service_proxy_configuration)) {}

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

// Builds a DM request and sends it via the wrapped network fetcher. Raw fetch
// result will be translated into DM request result for external callback.
// Do not reuse this class for multiple requests, as the class maintains the
// intermediate state of the wrapped network request.
class DMFetch : public base::RefCountedThreadSafe<DMFetch> {
 public:
  enum class TokenType {
    kEnrollmentToken,
    kDMToken,
  };

  using Callback =
      base::OnceCallback<void(DMClient::RequestResult result,
                              std::unique_ptr<std::string> response_body)>;

  DMFetch(std::unique_ptr<DMClient::Configurator> config,
          scoped_refptr<DMStorage> storage);
  DMFetch(const DMFetch&) = delete;
  DMFetch& operator=(const DMFetch&) = delete;

  const DMClient::Configurator* config() const { return config_.get(); }

  // Returns the storage where this client saves the data from DM server.
  scoped_refptr<DMStorage> storage() const { return storage_; }

  void PostRequest(const std::string& request_type,
                   TokenType token_type,
                   const std::string& request_data,
                   Callback callback);

 private:
  friend class base::RefCountedThreadSafe<DMFetch>;

  ~DMFetch();

  // Get the authorization token string.
  std::string BuildTokenString(TokenType type) const;

  // Gets the full request URL to DM server for the given request type.
  // Additional device specific values, such as device ID, platform etc. will
  // be appended to the URL as query parameters.
  GURL BuildURL(const std::string& request_type) const;

  // Callback functions for the URLFetcher.
  void OnRequestStarted(int response_code, int64_t content_length);
  void OnRequestProgress(int64_t current);
  void OnRequestComplete(std::unique_ptr<std::string> response_body,
                         int net_error,
                         const std::string& header_etag,
                         const std::string& header_x_cup_server_proof,
                         int64_t xheader_retry_after_sec);

  std::unique_ptr<DMClient::Configurator> config_;
  scoped_refptr<DMStorage> storage_;

  std::unique_ptr<update_client::NetworkFetcher> network_fetcher_;
  int http_status_code_ = 0;

  Callback callback_;
  SEQUENCE_CHECKER(sequence_checker_);
};

DMFetch::DMFetch(std::unique_ptr<DMClient::Configurator> config,
                 scoped_refptr<DMStorage> storage)
    : config_(std::move(config)),
      storage_(storage),
      network_fetcher_(config_->CreateNetworkFetcher()) {}

DMFetch::~DMFetch() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

GURL DMFetch::BuildURL(const std::string& request_type) const {
  GURL url(config_->GetDMServerUrl());
  url = AppendQueryParameter(url, kParamRequest, request_type);
  url = AppendQueryParameter(url, kParamAppType, kValueAppType);
  url = AppendQueryParameter(url, kParamAgent, config_->GetAgentParameter());
  url = AppendQueryParameter(url, kParamPlatform,
                             config_->GetPlatformParameter());
  return AppendQueryParameter(url, kParamDeviceID, storage_->GetDeviceID());
}

std::string DMFetch::BuildTokenString(TokenType type) const {
  switch (type) {
    case TokenType::kEnrollmentToken:
      return base::StringPrintf("%s token=%s", kRegistrationTokenType,
                                storage_->GetEnrollmentToken().c_str());

    case TokenType::kDMToken:
      return base::StringPrintf("%s token=%s", kDMTokenType,
                                storage_->GetDmToken().c_str());
  }
}

void DMFetch::PostRequest(const std::string& request_type,
                          TokenType token_type,
                          const std::string& request_data,
                          Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callback_ = std::move(callback);

  const bool is_registering = token_type == TokenType::kEnrollmentToken;
  DMClient::RequestResult result = DMClient::RequestResult::kSuccess;
  if (storage_->IsDeviceDeregistered()) {
    result = DMClient::RequestResult::kDeregistered;
  } else if (storage_->GetDeviceID().empty()) {
    result = DMClient::RequestResult::kNoDeviceID;
  } else if (!network_fetcher_) {
    result = DMClient::RequestResult::kFetcherError;
  } else if (request_data.empty()) {
    result = DMClient::RequestResult::kNoPayload;
  } else if (is_registering) {
    if (storage_->GetEnrollmentToken().empty()) {
      result = DMClient::RequestResult::kNotManaged;
    } else if (!storage_->GetDmToken().empty()) {
      result = DMClient::RequestResult::kAlreadyRegistered;
    }
  } else if (storage_->GetDmToken().empty()) {
    result = DMClient::RequestResult::kNoDMToken;
  }

  if (result != DMClient::RequestResult::kSuccess) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), result,
                                  std::make_unique<std::string>()));
    return;
  }

  const base::flat_map<std::string, std::string> additional_headers = {
      {kAuthorizationHeader, BuildTokenString(token_type)},
  };

  network_fetcher_->PostRequest(
      BuildURL(request_type), request_data, kDMContentType, additional_headers,
      base::BindOnce(&DMFetch::OnRequestStarted, base::Unretained(this)),
      base::BindRepeating(&DMFetch::OnRequestProgress, base::Unretained(this)),
      base::BindOnce(&DMFetch::OnRequestComplete, base::Unretained(this)));
}

void DMFetch::OnRequestStarted(int response_code, int64_t content_length) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "DM request is sent to server, status: " << response_code
          << ". Content length: " << content_length << ".";
  http_status_code_ = response_code;
}

void DMFetch::OnRequestProgress(int64_t current) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "DM request progress made, current bytes: " << current;
}

void DMFetch::OnRequestComplete(std::unique_ptr<std::string> response_body,
                                int net_error,
                                const std::string& header_etag,
                                const std::string& header_x_cup_server_proof,
                                int64_t xheader_retry_after_sec) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DMClient::RequestResult result = DMClient::RequestResult::kSuccess;
  if (net_error != 0) {
    VLOG(1) << "DM request failed due to net error: " << net_error;
    result = DMClient::RequestResult::kNetworkError;
  } else if (http_status_code_ == kHTTPStatusGone) {
    if (ShouldDeleteDmToken(*response_body)) {
      storage_->DeleteDMToken();
      result = DMClient::RequestResult::kNoDMToken;
      VLOG(1) << "Device is now de-registered by deleting the DM token.";
    } else {
      storage_->InvalidateDMToken();
      result = DMClient::RequestResult::kDeregistered;
      VLOG(1) << "Device is now de-registered by invalidating the DM token.";
    }
  } else if (http_status_code_ != kHTTPStatusOK) {
    VLOG(1) << "DM request failed due to HTTP error: " << http_status_code_;
    result = DMClient::RequestResult::kHttpError;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), result, std::move(response_body)));
}

void OnDMRegisterRequestComplete(scoped_refptr<DMFetch> dm_fetch,
                                 DMClient::RegisterCallback callback,
                                 DMClient::RequestResult result,
                                 std::unique_ptr<std::string> response_body) {
  if (result == DMClient::RequestResult::kSuccess) {
    const std::string dm_token =
        ParseDeviceRegistrationResponse(*response_body);
    if (dm_token.empty()) {
      VLOG(1) << "Failed to parse DM token from registration response.";
      result = DMClient::RequestResult::kUnexpectedResponse;
    } else {
      VLOG(1) << "Register request completed, got DM token: " << dm_token;
      if (!dm_fetch->storage()->StoreDmToken(dm_token)) {
        result = DMClient::RequestResult::kSerializationError;
      }
    }
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void OnDMPolicyFetchRequestComplete(
    scoped_refptr<DMFetch> dm_fetch,
    DMClient::PolicyFetchCallback callback,
    std::unique_ptr<CachedPolicyInfo> cached_info,
    DMClient::RequestResult result,
    std::unique_ptr<std::string> response_body) {
  std::vector<PolicyValidationResult> validation_results;
  scoped_refptr<DMStorage> storage = dm_fetch->storage();
  if (result == DMClient::RequestResult::kSuccess) {
    DMPolicyMap policies = ParsePolicyFetchResponse(
        *response_body, *cached_info, storage->GetDmToken(),
        storage->GetDeviceID(), validation_results);

    if (policies.empty()) {
      result = DMClient::RequestResult::kUnexpectedResponse;
    } else {
      VLOG(1) << "Policy fetch request completed, got " << policies.size()
              << " new policies.";
      if (!storage->PersistPolicies(policies)) {
        result = DMClient::RequestResult::kSerializationError;
      }
    }
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), result, validation_results));
}

void OnDMPolicyValidationReportRequestComplete(
    scoped_refptr<DMFetch> dm_fetch,
    DMClient::PolicyValidationReportCallback callback,
    DMClient::RequestResult result,
    std::unique_ptr<std::string> response_body) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

}  // namespace

void DMClient::RegisterDevice(std::unique_ptr<Configurator> config,
                              scoped_refptr<DMStorage> storage,
                              RegisterCallback callback) {
  auto dm_fetch = base::MakeRefCounted<DMFetch>(std::move(config), storage);
  dm_fetch->PostRequest(kRegistrationRequestType,
                        DMFetch::TokenType::kEnrollmentToken,
                        GetRegisterBrowserRequestData(),
                        base::BindOnce(OnDMRegisterRequestComplete, dm_fetch,
                                       std::move(callback)));
}

void DMClient::FetchPolicy(std::unique_ptr<Configurator> config,
                           scoped_refptr<DMStorage> storage,
                           PolicyFetchCallback callback) {
  if (!storage->CanPersistPolicies()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  DMClient::RequestResult::kSerializationError,
                                  std::vector<PolicyValidationResult>()));
    return;
  }

  auto dm_fetch = base::MakeRefCounted<DMFetch>(std::move(config), storage);
  std::unique_ptr<CachedPolicyInfo> cached_info =
      dm_fetch->storage()->GetCachedPolicyInfo();
  const std::string request_data =
      GetPolicyFetchRequestData(kGoogleUpdateMachineLevelApps, *cached_info);
  dm_fetch->PostRequest(
      kPolicyFetchRequestType, DMFetch::TokenType::kDMToken, request_data,
      base::BindOnce(&OnDMPolicyFetchRequestComplete, dm_fetch,
                     std::move(callback), std::move(cached_info)));
}

void DMClient::ReportPolicyValidationErrors(
    std::unique_ptr<Configurator> config,
    scoped_refptr<DMStorage> storage,
    const PolicyValidationResult& validation_result,
    PolicyValidationReportCallback callback) {
  auto dm_fetch = base::MakeRefCounted<DMFetch>(std::move(config), storage);
  dm_fetch->PostRequest(
      kValidationReportRequestType, DMFetch::TokenType::kDMToken,
      GetPolicyValidationReportRequestData(validation_result),
      base::BindOnce(&OnDMPolicyValidationReportRequestComplete, dm_fetch,
                     std::move(callback)));
}

std::unique_ptr<DMClient::Configurator> DMClient::CreateDefaultConfigurator(
    absl::optional<PolicyServiceProxyConfiguration>
        policy_service_proxy_configuration) {
  return std::make_unique<DefaultConfigurator>(
      policy_service_proxy_configuration);
}

}  // namespace updater
