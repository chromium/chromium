// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_policy.h"

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/policy/test_support/signature_provider.h"
#include "components/policy/test_support/test_server_helpers.h"
#include "crypto/rsa_private_key.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;

namespace em = enterprise_management;

namespace policy {

// As policy test server can be used not only for regular managed users,
// but also for unicorn users, we need to handle some policy aspects for
// them in a special way.
inline constexpr char kUnicornUsersDomain[] = "gmail.com";

RequestHandlerForPolicy::RequestHandlerForPolicy(
    EmbeddedPolicyTestServer* parent)
    : EmbeddedPolicyTestServer::RequestHandler(parent) {}

RequestHandlerForPolicy::~RequestHandlerForPolicy() = default;

std::string RequestHandlerForPolicy::RequestType() {
  return dm_protocol::kValueRequestPolicy;
}

std::unique_ptr<HttpResponse> RequestHandlerForPolicy::HandleRequest(
    const HttpRequest& request) {
  const base::flat_set<std::string> kCloudPolicyTypes{
      dm_protocol::kChromeDevicePolicyType,
      dm_protocol::kChromeExtensionPolicyType,
      dm_protocol::kChromeMachineLevelUserCloudPolicyType,
      dm_protocol::kChromeMachineLevelExtensionCloudPolicyType,
      dm_protocol::kChromePublicAccountPolicyType,
      dm_protocol::kChromeSigninExtensionPolicyType,
      dm_protocol::kChromeUserPolicyType,
      dm_protocol::kGoogleUpdateMachineLevelAppsPolicyType,
      dm_protocol::kGoogleUpdateMachineLevelOmahaPolicyType,
  };
  const base::flat_set<std::string> kExtensionPolicyTypes{
      dm_protocol::kChromeExtensionPolicyType,
      dm_protocol::kChromeMachineLevelExtensionCloudPolicyType,
      dm_protocol::kChromeSigninExtensionPolicyType,
  };

  std::string request_device_token;
  if (!GetDeviceTokenFromRequest(request, &request_device_token)) {
    return CreateHttpResponse(net::HTTP_UNAUTHORIZED, "Invalid device token.");
  }

  em::DeviceManagementResponse device_management_response;
  const ClientStorage::ClientInfo* client_info =
      client_storage()->GetClientOrNull(
          KeyValueFromUrl(request.GetURL(), dm_protocol::kParamDeviceID));
  if (!client_info || client_info->device_token != request_device_token) {
    device_management_response.add_error_detail(
        policy_storage()->error_detail());
    return CreateHttpResponse(net::HTTP_GONE, device_management_response);
  }

  em::DeviceManagementRequest device_management_request;
  device_management_request.ParseFromString(request.content);

  // If this is a public account request, use the |settings_entity_id| from the
  // request as the |username|. This is required to validate policy for
  // extensions in device-local accounts.
  ClientStorage::ClientInfo modified_client_info(*client_info);
  std::vector<em::PolicyFetchRequest> fetch_requests;
  for (const auto& fetch_request :
       device_management_request.policy_request().requests()) {
    if (fetch_request.policy_type() ==
        dm_protocol::kChromePublicAccountPolicyType) {
      modified_client_info.username = fetch_request.settings_entity_id();
      client_info = &modified_client_info;
    }

    if (fetch_request.policy_type() ==
        dm_protocol::kGoogleUpdateMachineLevelAppsPolicyType) {
      // The "google/machine-level-apps" policy type has a special behavior in
      // that the server should auto-expand it to fetch requests for
      // "google/machine-level-omaha", "google/chrome/machine-level-user", and
      // "google/chrome/machine-level-extension".
      for (const auto& new_policy_type :
           {dm_protocol::kGoogleUpdateMachineLevelOmahaPolicyType,
            dm_protocol::kChromeMachineLevelUserCloudPolicyType,
            dm_protocol::kChromeMachineLevelExtensionCloudPolicyType}) {
        em::PolicyFetchRequest new_fetch_request = fetch_request;
        new_fetch_request.set_policy_type(new_policy_type);
        fetch_requests.push_back(new_fetch_request);
      }
    } else {
      fetch_requests.push_back(fetch_request);
    }
  }

  for (const auto& fetch_request : fetch_requests) {
    const std::string& policy_type = fetch_request.policy_type();
    // TODO(crbug.com/40773420): Add other policy types as needed.
    if (!base::Contains(kCloudPolicyTypes, policy_type)) {
      return CreateHttpResponse(
          net::HTTP_BAD_REQUEST,
          base::StringPrintf("Invalid policy_type: %s", policy_type.c_str()));
    }

    std::string error_msg;
    if (base::Contains(kExtensionPolicyTypes, policy_type)) {
      if (!ProcessCloudPolicyForExtensions(
              fetch_request, *client_info,
              device_management_response.mutable_policy_response(),
              &error_msg)) {
        return CreateHttpResponse(net::HTTP_BAD_REQUEST, error_msg);
      }
    } else if (!ProcessCloudPolicy(
                   fetch_request, *client_info,
                   device_management_response.mutable_policy_response()
                       ->add_responses(),
                   &error_msg)) {
      return CreateHttpResponse(net::HTTP_BAD_REQUEST, error_msg);
    }
  }

  return CreateHttpResponse(net::HTTP_OK, device_management_response);
}

bool RequestHandlerForPolicy::ProcessCloudPolicy(
    const em::PolicyFetchRequest& fetch_request,
    const ClientStorage::ClientInfo& client_info,
    em::PolicyFetchResponse* fetch_response,
    std::string* error_msg) {
  const std::string& policy_type = fetch_request.policy_type();
  if (client_info.allowed_policy_types.find(policy_type) ==
      client_info.allowed_policy_types.end()) {
    error_msg->assign("Policy type not allowed for token: ")
        .append(policy_type);
    return false;
  }

  // Determine the current key on the client.
  const SignatureProvider::SigningKey* client_key = nullptr;
  const SignatureProvider* signature_provider =
      policy_storage()->signature_provider();
  int public_key_version = fetch_request.public_key_version();
  if (fetch_request.has_public_key_version()) {
    client_key = signature_provider->GetKeyByVersion(public_key_version);
    if (!client_key) {
      error_msg->assign(base::StringPrintf("Invalid public key version: %d",
                                           public_key_version));
      return false;
    }
  }

  // Choose the key for signing the policy.
  int signing_key_version = signature_provider->current_key_version();
  if (fetch_request.has_public_key_version() &&
      signature_provider->rotate_keys()) {
    signing_key_version = public_key_version + 1;
  }
  const SignatureProvider::SigningKey* signing_key =
      signature_provider->GetKeyByVersion(signing_key_version);
  if (!signing_key) {
    error_msg->assign(base::StringPrintf(
        "Can't find signin key for version: %d", signing_key_version));
    return false;
  }

  em::PolicyData policy_data;
  policy_data.set_policy_type(policy_type);
  policy_data.set_timestamp(
      policy_storage()->timestamp().is_null()
          ? base::Time::Now().InMillisecondsSinceUnixEpoch()
          : policy_storage()->timestamp().InMillisecondsSinceUnixEpoch());
  policy_data.set_request_token(client_info.device_token);
  policy_data.set_policy_value(policy_storage()->GetPolicyPayload(
      policy_type, fetch_request.settings_entity_id()));
  policy_data.set_settings_entity_id(fetch_request.settings_entity_id());
  policy_data.set_machine_name(client_info.machine_name);
  policy_data.set_service_account_identity(
      policy_storage()->service_account_identity().empty()
          ? "policy-testserver-service-account-identity@gmail.com"
          : policy_storage()->service_account_identity());
  policy_data.set_device_id(client_info.device_id);
  std::string username =
      client_info.username.value_or(policy_storage()->policy_user().empty()
                                        ? kDefaultUsername
                                        : policy_storage()->policy_user());
  policy_data.set_username(username);

  std::string domain = gaia::ExtractDomainName(gaia::SanitizeEmail(username));

  if (domain != kUnicornUsersDomain) {
    // Unicorn users don't have "managed by" field.
    policy_data.set_managed_by(domain);
  }
  policy_data.set_policy_invalidation_topic(
      policy_storage()->policy_invalidation_topic());

  if (fetch_request.signature_type() != em::PolicyFetchRequest::NONE) {
    policy_data.set_public_key_version(signing_key_version);
  }

  if (policy_type == dm_protocol::kChromeUserPolicyType ||
      policy_type == dm_protocol::kChromePublicAccountPolicyType) {
    std::vector<std::string> user_affiliation_ids =
        policy_storage()->user_affiliation_ids();
    if (!user_affiliation_ids.empty()) {
      for (const std::string& user_affiliation_id : user_affiliation_ids) {
        policy_data.add_user_affiliation_ids(user_affiliation_id);
      }
    }
    if (policy_storage()->metrics_log_segment()) {
      policy_data.set_metrics_log_segment(
          policy_storage()->metrics_log_segment().value());
    }
  } else if (policy_type == dm_protocol::kChromeDevicePolicyType) {
    std::vector<std::string> device_affiliation_ids =
        policy_storage()->device_affiliation_ids();
    if (!device_affiliation_ids.empty()) {
      for (const std::string& device_affiliation_id : device_affiliation_ids) {
        policy_data.add_device_affiliation_ids(device_affiliation_id);
      }
    }
    if (policy_storage()->market_segment()) {
      policy_data.set_market_segment(
          policy_storage()->market_segment().value());
    }
  }

  std::string directory_api_id = policy_storage()->directory_api_id();
  if (!directory_api_id.empty()) {
    policy_data.set_directory_api_id(directory_api_id);
  }

  policy_data.SerializeToString(fetch_response->mutable_policy_data());

  if (fetch_request.signature_type() != em::PolicyFetchRequest::NONE) {
    // Sign the serialized policy data.
    if (!signing_key->Sign(fetch_response->policy_data(),
                           fetch_request.signature_type(),
                           fetch_response->mutable_policy_data_signature())) {
      error_msg->assign("Error signing policy_data");
      return false;
    }

    if (!fetch_request.has_public_key_version() ||
        public_key_version != signing_key_version) {
      fetch_response->set_new_public_key(signing_key->public_key());

      // Add the new public key verification data.
      em::PublicKeyVerificationData new_signing_key_verification_data;
      new_signing_key_verification_data.set_new_public_key(
          signing_key->public_key());
      new_signing_key_verification_data.set_domain(domain);
      new_signing_key_verification_data.set_new_public_key_version(
          signing_key_version);
      std::string new_signing_key_verification_data_as_string;
      CHECK(new_signing_key_verification_data.SerializeToString(
          &new_signing_key_verification_data_as_string));
      fetch_response->set_new_public_key_verification_data(
          new_signing_key_verification_data_as_string);
      CHECK(signature_provider->SignVerificationData(
          new_signing_key_verification_data_as_string,
          fetch_response
              ->mutable_new_public_key_verification_data_signature()));
    }

    // Set the verification signature appropriate for the policy domain.
    // TODO(http://crbug.com/328038): Use the enrollment domain for public
    // accounts when we add key validation for ChromeOS.
    if (!signing_key->GetSignatureForDomain(
            domain,
            fetch_response
                ->mutable_new_public_key_verification_signature_deprecated())) {
      error_msg->assign(
          base::StringPrintf("No signature for domain: %s", domain.c_str()));
      return false;
    }

    if (client_key &&
        !client_key->Sign(fetch_response->new_public_key(),
                          fetch_request.signature_type(),
                          fetch_response->mutable_new_public_key_signature())) {
      error_msg->assign("Error signing new_public_key");
      return false;
    }

    fetch_response->set_policy_data_signature_type(
        fetch_request.signature_type());
  }

  return true;
}

bool RequestHandlerForPolicy::ProcessCloudPolicyForExtensions(
    const em::PolicyFetchRequest& fetch_request,
    const ClientStorage::ClientInfo& client_info,
    em::DevicePolicyResponse* response,
    std::string* error_msg) {
  // Send one PolicyFetchResponse for each extension configured on the server as
  // the client does not actually tell us which extensions it has installed to
  // protect user privacy.
  std::vector<std::string> ids =
      policy_storage()->GetEntityIdsForType(fetch_request.policy_type());
  for (const std::string& id : ids) {
    em::PolicyFetchRequest fetch_request_with_id;
    fetch_request_with_id.CopyFrom(fetch_request);
    fetch_request_with_id.set_settings_entity_id(id);
    if (!ProcessCloudPolicy(fetch_request_with_id, client_info,
                            response->add_responses(), error_msg)) {
      return false;
    }
  }

  return true;
}

}  // namespace policy
