// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/carrier_lock/provisioning_config_fetcher_impl.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/values.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace ash::carrier_lock {

namespace {

// const values
constexpr size_t kMaxProvisioningResponseSizeBytes = 1 << 20;  // 1MB;
constexpr base::TimeDelta kRequestTimeoutSeconds = base::Seconds(30);
const char kAndroidId[] = "123";

// Pixel provisioning server data
const char kProvisioningRecord[] = "deviceProvisioningRecord";
const char kProvisioningConfig[] = "deviceProvisioningConfig";

const char kProvisioningUrl[] =
    "https://afwprovisioning-pa.googleapis.com"
    "/v1/get_device_provisioning_record";

// Traffic annotation for configuration request
const net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation(
        "carrier_lock_manager_fetch_configuration",
        R"(
        semantics {
          sender: "Carrier Lock manager"
          description:
            "Request Carrier Lock configuration to setup modem locks."
          trigger: "Carrier Lock manager makes this network request every time "
                   "lock configuration needs to be updated on modem."
          data: "FCM token, manufacturer, model, serial number, IMEI, device id"
                "and API key."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
                email: "cros-cellular-core@google.com"
            }
          }
          user_data {
            type: DEVICE_ID
            type: HW_OS_INFO
            type: ACCESS_TOKEN
          }
          last_reviewed: "2023-10-24"
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled in settings."
          policy_exception_justification: "Carrier Lock is always enforced."
        })");

}  // namespace

ProvisioningConfigFetcherImpl::ProvisioningConfigFetcherImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {}

ProvisioningConfigFetcherImpl::~ProvisioningConfigFetcherImpl() = default;

void ProvisioningConfigFetcherImpl::RequestConfig(
    const std::string& serial,
    const std::string& imei,
    const std::string& manufacturer,
    const std::string& model,
    const std::string& fcm_token,
    Callback callback) {
  if (config_callback_) {
    LOG(ERROR)
        << "ProvisioningConfigFetcherImpl cannot handle multiple requests.";
    std::move(callback).Run(Result::kHandlerBusy);
    return;
  }

  config_callback_ = std::move(callback);

  // Prepare message header
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kProvisioningUrl);
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->headers.SetHeader("x-goog-api-key",
                                      google_apis::GetAPIKey());
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      "application/json, */*;q=0.5");
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      "application/json");

  // Prepare request body
  base::Value::Dict request;
  base::Value::Dict device;
  std::string request_body;
  request.Set("android_id", kAndroidId);
  request.Set("gcm_registration_id", fcm_token);
  device.Set("manufacturer", manufacturer);
  device.Set("model", model);
  device.Set("serialNumber", serial);
  device.Set("chromeOsAttestedDeviceId", serial);
  device.Set("imei", imei);
  request.Set("deviceIdentifier", std::move(device));
  base::JSONWriter::Write(request, &request_body);

  // Send message using URLLoader
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  if (!simple_url_loader_ || !url_loader_factory_) {
    LOG(ERROR) << "Failed to create URL loader";
    ReturnError(Result::kInitializationFailed);
    return;
  }

  simple_url_loader_->AttachStringForUpload(request_body, "application/json");
  simple_url_loader_->SetTimeoutDuration(kRequestTimeoutSeconds);
  simple_url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&ProvisioningConfigFetcherImpl::OnDownloadToStringComplete,
                     base::Unretained(this)),
      kMaxProvisioningResponseSizeBytes);
}

std::string ProvisioningConfigFetcherImpl::GetFcmTopic() {
  return provision_config_.sim_lock_config().gcm_topic();
}

std::string ProvisioningConfigFetcherImpl::GetSignedConfig() {
  return provision_config_.sim_lock_config().signed_configuration();
}

::carrier_lock::CarrierRestrictionsMode
ProvisioningConfigFetcherImpl::GetRestrictionMode() {
  return provision_config_.sim_lock_config().carrier_restrictions_mode();
}

RestrictedNetworks ProvisioningConfigFetcherImpl::GetNumberOfNetworks() {
  RestrictedNetworks result;
  result.allowed = provision_config_.sim_lock_config().allowed_networks_size();
  result.disallowed =
      provision_config_.sim_lock_config().disallowed_networks_size();
  return result;
}

void ProvisioningConfigFetcherImpl::OnDownloadToStringComplete(
    std::unique_ptr<std::string> response_body) {
  simple_url_loader_.reset();
  if (!response_body) {
    LOG(ERROR) << "Provisioning response body is empty";
    ReturnError(Result::kConnectionError);
    return;
  }

  base::JSONReader::Result response_result =
      base::JSONReader::ReadAndReturnValueWithError(*response_body);
  if (!response_result.has_value()) {
    LOG(ERROR) << "Provisioning JSONReader failed: "
               << response_result.error().message;
    ReturnError(Result::kInvalidResponse);
    return;
  }

  if (!response_result.value().is_dict()) {
    LOG(ERROR) << "Provisioning response has unexpected type : "
               << base::StringPrintf("%u", static_cast<unsigned int>(
                                               response_result.value().type()));
    ReturnError(Result::kInvalidResponse);
    return;
  }
  base::Value::Dict response_value =
      std::move(response_result.value().GetDict());

  base::Value::Dict* response_record =
      response_value.FindDict(kProvisioningRecord);
  if (!response_record) {
    LOG(ERROR) << "Provisioning record not found";
    ReturnError(Result::kNoLockConfiguration);
    return;
  }

  base::Value* response_config = response_record->Find(kProvisioningConfig);
  if (!response_config || !response_config->is_string()) {
    LOG(ERROR) << "Provisioning config not found or not a string";
    ReturnError(Result::kNoLockConfiguration);
    return;
  }

  std::string response_string;
  if (!base::Base64Decode(response_config->GetString(), &response_string)) {
    LOG(ERROR) << "Provisioning config decoding failed";
    ReturnError(Result::kInvalidConfiguration);
    return;
  }

  if (!provision_config_.ParseFromString(response_string)) {
    LOG(ERROR) << "Provisioning device config parse failed";
    ReturnError(Result::kInvalidConfiguration);
    return;
  }

  std::move(config_callback_).Run(Result::kSuccess);
}

void ProvisioningConfigFetcherImpl::ReturnError(Result err) {
  std::move(config_callback_).Run(err);
}

}  // namespace ash::carrier_lock
