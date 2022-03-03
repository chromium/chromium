// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cup_impl.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/switches.h"
#include "components/client_update_protocol/ecdsa.h"

namespace {

// This is an ECDSA prime256v1 named-curve key.
constexpr int kKeyVersion = 11;
constexpr char kKeyPubBytesBase64[] =
    "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEgH30WRJf4g6I2C1FKsBQF3qHANLw"
    "thwYsNt2PWTDQBS0ufSRE83piOPoJQcePzTkMfbghjnZerDjLJhBsDkfFg==";

absl::optional<std::string> GetKey(const std::string& key_bytes_base64) {
  std::string result;
  return base::Base64Decode(key_bytes_base64, &result)
             ? absl::optional<std::string>(result)
             : absl::nullopt;
}

}  // namespace

namespace autofill_assistant {

namespace cup {

int CUPImpl::GetKeyVersion() {
  int key_version = kKeyVersion;
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAutofillAssistantCupKeyVersion)) {
    return key_version;
  }

  if (!base::StringToInt(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kAutofillAssistantCupKeyVersion),
          &key_version)) {
    LOG(ERROR) << "Error parsing command line flag "
               << switches::kAutofillAssistantCupKeyVersion << ": not a number";
    // If CLI key version is not valid, continue with the default one.
    return kKeyVersion;
  }
  return key_version;
}

std::string CUPImpl::GetPublicKey() {
  absl::optional<std::string> pub_key = GetKey(kKeyPubBytesBase64);
  // The default key specified in |kKeyPubBytesBase64| must be valid base64.
  DCHECK(pub_key.has_value());

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAutofillAssistantCupPublicKeyBase64)) {
    return *pub_key;
  }

  absl::optional<std::string> switch_pub_key =
      GetKey(base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAutofillAssistantCupPublicKeyBase64));
  if (!switch_pub_key.has_value()) {
    LOG(ERROR) << "Error parsing command line flag "
               << switches::kAutofillAssistantCupPublicKeyBase64
               << ": not a valid base64 string";
    // If CLI public key is not valid, continue with the default one.
    return *pub_key;
  }
  return *switch_pub_key;
}

std::unique_ptr<client_update_protocol::Ecdsa> CUPImpl::CreateQuerySigner() {
  return client_update_protocol::Ecdsa::Create(GetKeyVersion(), GetPublicKey());
}

CUPImpl::CUPImpl(std::unique_ptr<client_update_protocol::Ecdsa> query_signer,
                 RpcType rpc_type)
    : query_signer_{std::move(query_signer)}, rpc_type_{rpc_type} {
  DCHECK(query_signer_);
}

CUPImpl::~CUPImpl() = default;

std::string CUPImpl::PackAndSignRequest(const std::string& original_request) {
  if (rpc_type_ != RpcType::GET_ACTIONS) {
    // Failsafe in case the method is called for a non-supported |rpc_type|.
    return original_request;
  }
  return PackGetActionsRequest(original_request);
}

absl::optional<std::string> CUPImpl::UnpackResponse(
    const std::string& original_response) {
  if (rpc_type_ != RpcType::GET_ACTIONS) {
    // Failsafe in case the method is called for a non-supported |rpc_type|.
    return original_response;
  }
  return UnpackGetActionsResponse(original_response);
}

std::string CUPImpl::PackGetActionsRequest(
    const std::string& original_request) {
  autofill_assistant::ScriptActionRequestProto actions_request;
  actions_request.mutable_cup_data()->set_request(original_request);

  client_update_protocol::Ecdsa::RequestParameters request_parameters =
      query_signer_->SignRequest(original_request);
  actions_request.mutable_cup_data()->set_query_cup2key(
      request_parameters.query_cup2key);
  actions_request.mutable_cup_data()->set_hash_hex(request_parameters.hash_hex);

  std::string serialized_request;
  actions_request.SerializeToString(&serialized_request);
  return serialized_request;
}

absl::optional<std::string> CUPImpl::UnpackGetActionsResponse(
    const std::string& original_response) {
  autofill_assistant::ActionsResponseProto actions_response;
  if (!actions_response.ParseFromString(original_response)) {
    LOG(ERROR) << "Failed to parse server response";
    Metrics::RecordCupRpcVerificationEvent(
        Metrics::CupRpcVerificationEvent::PARSING_FAILED);
    return absl::nullopt;
  }

  std::string serialized_response = actions_response.cup_data().response();
  if (!query_signer_->ValidateResponse(
          serialized_response, actions_response.cup_data().ecdsa_signature())) {
    LOG(ERROR) << "CUP RPC response verification failed";
    Metrics::RecordCupRpcVerificationEvent(
        Metrics::CupRpcVerificationEvent::VERIFICATION_FAILED);
    return absl::nullopt;
  }

  Metrics::RecordCupRpcVerificationEvent(
      Metrics::CupRpcVerificationEvent::VERIFICATION_SUCCEEDED);
  return serialized_response;
}

client_update_protocol::Ecdsa& CUPImpl::GetQuerySigner() {
  return *query_signer_.get();
}

}  // namespace cup

}  // namespace autofill_assistant
