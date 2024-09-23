// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/device_management/dm_client.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <ostream>
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
#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"
#include "chrome/updater/device_management/dm_message.h"
#include "chrome/updater/device_management/dm_response_validator.h"
#include "chrome/updater/net/network.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/util.h"
#include "components/update_client/network.h"
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
  DefaultConfigurator(const GURL& server_url,
                      std::optional<PolicyServiceProxyConfiguration>
                          policy_service_proxy_configuration)
      : server_url_(server_url),
        policy_service_proxy_configuration_(
            std::move(policy_service_proxy_configuration)) {}

  ~DefaultConfigurator() override = default;

  GURL GetDMServerUrl() const override { return server_url_; }

  std::string GetAgentParameter() const override {
    return GetUpdaterUserAgent();
  }

  std::string GetPlatformParameter() const override {
    int32_t major = 0;
    int32_t minor = 0;
    int32_t bugfix = 0;
    base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
    return base::StringPrintf(
        "%s|%s|%d.%d.%d", base::SysInfo::OperatingSystemName().c_str(),
        base::SysInfo::OperatingSystemArchitecture().c_str(), major, minor,
        bugfix);
  }

  std::unique_ptr<update_client::NetworkFetcher> CreateNetworkFetcher()
      const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!network_fetcher_factory_) {
      network_fetcher_factory_ = base::MakeRefCounted<NetworkFetcherFactory>(
          policy_service_proxy_configuration_);
    }
    return network_fetcher_factory_->Create();
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  const GURL server_url_;
  const std::optional<PolicyServiceProxyConfiguration>
      policy_service_proxy_configuration_;
  mutable scoped_refptr<update_client::NetworkFetcherFactory>
      network_fetcher_factory_;
};

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
          scoped_refptr<device_management_storage::DMStorage> storage);
  DMFetch(const DMFetch&) = delete;
  DMFetch& operator=(const DMFetch&) = delete;

  const DMClient::Configurator* config() const { return config_.get(); }

  // Returns the storage where this client saves the data from DM server.
  scoped_refptr<device_management_storage::DMStorage> storage() const {
    return storage_;
  }

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
  scoped_refptr<device_management_storage::DMStorage> storage_;

  std::unique_ptr<update_client::NetworkFetcher> network_fetcher_;
  int http_status_code_ = 0;

  Callback callback_;
  SEQUENCE_CHECKER(sequence_checker_);
};

DMFetch::DMFetch(std::unique_ptr<DMClient::Configurator> config,
                 scoped_refptr<device_management_storage::DMStorage> storage)
    : config_(std::move(config)), storage_(std::move(storage)) {}

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

  DMClient::RequestResult result = [&] {
    if (storage_->IsDeviceDeregistered()) {
      return DMClient::RequestResult::kDeregistered;
    }
    if (storage_->GetDeviceID().empty()) {
      return DMClient::RequestResult::kNoDeviceID;
    }
    if (request_data.empty()) {
      return DMClient::RequestResult::kNoPayload;
    }
    if (token_type == TokenType::kEnrollmentToken) {
      if (storage_->GetEnrollmentToken().empty()) {
        return DMClient::RequestResult::kNotManaged;
      }
      if (!storage_->GetDmToken().empty()) {
        return DMClient::RequestResult::kAlreadyRegistered;
      }
      storage_->RemoveAllPolicies();
      return DMClient::RequestResult::kSuccess;
    }
    if (storage_->GetDmToken().empty()) {
      return DMClient::RequestResult::kNoDMToken;
    }
    return DMClient::RequestResult::kSuccess;
  }();

  if (result == DMClient::RequestResult::kSuccess) {
    network_fetcher_ = config_->CreateNetworkFetcher();
    if (!network_fetcher_) {
      result = DMClient::RequestResult::kFetcherError;
    }
  }

  if (result != DMClient::RequestResult::kSuccess) {
    VLOG(1) << "DM request not sent: " << result;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), result,
                                  std::make_unique<std::string>()));
    return;
  }
  network_fetcher_->PostRequest(
      BuildURL(request_type), request_data, kDMContentType,
      {{kAuthorizationHeader, BuildTokenString(token_type)}},
      base::BindRepeating(&DMFetch::OnRequestStarted, base::Unretained(this)),
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
  VLOG(3) << "DM request progress made, current bytes: " << current;
}

void DMFetch::OnRequestComplete(std::unique_ptr<std::string> response_body,
                                int net_error,
                                const std::string& header_etag,
                                const std::string& header_x_cup_server_proof,
                                int64_t xheader_retry_after_sec) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << __func__;

  DMClient::RequestResult result = DMClient::RequestResult::kSuccess;
  if (net_error != 0) {
    VLOG(1) << "DM request failed due to net error: " << net_error;
    result = DMClient::RequestResult::kNetworkError;
  } else if (http_status_code_ == kHTTPStatusGone) {
    VLOG(1) << "Got response to delete/invalidate the DM token.";
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
  VLOG(2) << __func__ << ": result=" << result;
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
    std::unique_ptr<device_management_storage::CachedPolicyInfo> cached_info,
    DMClient::RequestResult result,
    std::unique_ptr<std::string> response_body) {
  VLOG(2) << __func__ << ": result=" << result;
  std::vector<PolicyValidationResult> validation_results;
  scoped_refptr<device_management_storage::DMStorage> storage =
      dm_fetch->storage();
  if (result == DMClient::RequestResult::kSuccess) {
    device_management_storage::DMPolicyMap policies = ParsePolicyFetchResponse(
        *response_body, *cached_info, storage->GetDmToken(),
        storage->GetDeviceID(), validation_results);

    if (policies.empty()) {
      VLOG(1) << "No policy passes the validation, reset the policy cache.";
      result = DMClient::RequestResult::kUnexpectedResponse;
      storage->RemoveAllPolicies();
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
  VLOG(2) << __func__ << ": result=" << result;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

}  // namespace

void DMClient::RegisterDevice(
    std::unique_ptr<Configurator> config,
    scoped_refptr<device_management_storage::DMStorage> storage,
    RegisterCallback callback) {
  VLOG(2) << __func__;
  auto dm_fetch = base::MakeRefCounted<DMFetch>(std::move(config), storage);
  dm_fetch->PostRequest(kRegistrationRequestType,
                        DMFetch::TokenType::kEnrollmentToken,
                        GetRegisterBrowserRequestData(),
                        base::BindOnce(OnDMRegisterRequestComplete, dm_fetch,
                                       std::move(callback)));
}

void DMClient::FetchPolicy(
    std::unique_ptr<Configurator> config,
    scoped_refptr<device_management_storage::DMStorage> storage,
    PolicyFetchCallback callback) {
  VLOG(2) << __func__;
  if (!storage->CanPersistPolicies()) {
    VLOG(2) << "Cannot persist policies.";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  DMClient::RequestResult::kSerializationError,
                                  std::vector<PolicyValidationResult>()));
    return;
  }

  auto dm_fetch = base::MakeRefCounted<DMFetch>(std::move(config), storage);
  std::unique_ptr<device_management_storage::CachedPolicyInfo> cached_info =
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
    scoped_refptr<device_management_storage::DMStorage> storage,
    const PolicyValidationResult& validation_result,
    PolicyValidationReportCallback callback) {
  VLOG(2) << __func__;
  auto dm_fetch = base::MakeRefCounted<DMFetch>(std::move(config), storage);
  dm_fetch->PostRequest(
      kValidationReportRequestType, DMFetch::TokenType::kDMToken,
      GetPolicyValidationReportRequestData(validation_result),
      base::BindOnce(&OnDMPolicyValidationReportRequestComplete, dm_fetch,
                     std::move(callback)));
}

std::unique_ptr<DMClient::Configurator> DMClient::CreateDefaultConfigurator(
    const GURL& server_url,
    std::optional<PolicyServiceProxyConfiguration>
        policy_service_proxy_configuration) {
  return std::make_unique<DefaultConfigurator>(
      server_url, policy_service_proxy_configuration);
}

std::ostream& operator<<(std::ostream& os,
                         const DMClient::RequestResult& result) {
#define SWITCH_ENTRY(p) \
  case p:               \
    return os << #p
  switch (result) {
    SWITCH_ENTRY(DMClient::RequestResult::kSuccess);
    SWITCH_ENTRY(DMClient::RequestResult::kNoDeviceID);
    SWITCH_ENTRY(DMClient::RequestResult::kAlreadyRegistered);
    SWITCH_ENTRY(DMClient::RequestResult::kNotManaged);
    SWITCH_ENTRY(DMClient::RequestResult::kDeregistered);
    SWITCH_ENTRY(DMClient::RequestResult::kNoDMToken);
    SWITCH_ENTRY(DMClient::RequestResult::kFetcherError);
    SWITCH_ENTRY(DMClient::RequestResult::kNetworkError);
    SWITCH_ENTRY(DMClient::RequestResult::kHttpError);
    SWITCH_ENTRY(DMClient::RequestResult::kSerializationError);
    SWITCH_ENTRY(DMClient::RequestResult::kUnexpectedResponse);
    SWITCH_ENTRY(DMClient::RequestResult::kNoPayload);
    SWITCH_ENTRY(DMClient::RequestResult::kNoDefaultDMStorage);
  }
#undef SWITCH_ENTRY
}

}  // namespace updater
