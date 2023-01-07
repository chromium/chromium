// Copyright 2021 The Chromium Authors
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
constexpr int kKeyVersion = 1;
constexpr char kKeyPubBytesBase64[] =
    "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEK2TXDqsaUceOfIJldE1T+RENfPZk848Se+"
    "8ODrfNFfIW4CK5qwgoCdE2xbJPkgivLHNnm1nk6LQM7mP6FgsOGg==";

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
  const int public_key_version = GetKeyVersion();

  VLOG(1) << "Resolved CUP public key version: '" << public_key_version << "'";

  return client_update_protocol::Ecdsa::Create(public_key_version,
                                               GetPublicKey());
}

CUPImpl::CUPImpl(std::unique_ptr<client_update_protocol::Ecdsa> query_signer,
                 RpcType rpc_type)
    : query_signer_{std::move(query_signer)}, rpc_type_{rpc_type} {
  DCHECK(query_signer_);
  VLOG(2) << "CUPImpl instance created";
}

CUPImpl::~CUPImpl() {
  VLOG(2) << "CUPImpl instance destroyed";
}

std::string CUPImpl::PackAndSignRequest(const std::string& original_request) {
  switch (rpc_type_) {
    case RpcType::GET_ACTIONS:
      return InternalPackAndSignRequest<ScriptActionRequestProto>(
          original_request);
    case RpcType::GET_NO_ROUNDTRIP_SCRIPTS_BY_HASH_PREFIX:
      return InternalPackAndSignRequest<
          GetNoRoundTripScriptsByHashPrefixRequestProto>(original_request);
    default:
      LOG(DFATAL) << "CUPImpl::PackAndSignRequest was called for "
                     "unsupported type. No packing was performed.";
      return original_request;
  }
}

absl::optional<std::string> CUPImpl::UnpackResponse(
    const std::string& original_response) {
  switch (rpc_type_) {
    case RpcType::GET_ACTIONS:
      return InternalUnpackResponse<ActionsResponseProto>(original_response);
    case RpcType::GET_NO_ROUNDTRIP_SCRIPTS_BY_HASH_PREFIX:
      return InternalUnpackResponse<
          GetNoRoundTripScriptsByHashPrefixResponseProto>(original_response);
    default:
      LOG(DFATAL) << "CUPImpl::UnpackResponse was called for "
                     "unsupported type. No unpacking was performed.";
      return original_response;
  }
}

template <class RequestProtoType>
std::string CUPImpl::InternalPackAndSignRequest(
    const std::string& original_request) {
  RequestProtoType actions_request;
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

template <class ResponseProtoType>
absl::optional<std::string> CUPImpl::InternalUnpackResponse(
    const std::string& original_response) {
  ResponseProtoType response;
  if (!response.ParseFromString(original_response)) {
    LOG(ERROR) << "Failed to parse server response";
    Metrics::RecordCupRpcVerificationEvent(
        Metrics::CupRpcVerificationEvent::PARSING_FAILED);
    return absl::nullopt;
  }

  if (response.cup_data().ecdsa_signature().empty()) {
    LOG(ERROR) << "Signature not provided for CUP RPC response";
    Metrics::RecordCupRpcVerificationEvent(
        Metrics::CupRpcVerificationEvent::EMPTY_SIGNATURE);
    return absl::nullopt;
  }

  std::string serialized_response = response.cup_data().response();
  if (!query_signer_->ValidateResponse(serialized_response,
                                       response.cup_data().ecdsa_signature())) {
    LOG(ERROR) << "CUP RPC response verification failed";
    Metrics::RecordCupRpcVerificationEvent(
        Metrics::CupRpcVerificationEvent::VERIFICATION_FAILED);
    return absl::nullopt;
  }

  VLOG(1) << "CUP RPC response verification succeeded";
  Metrics::RecordCupRpcVerificationEvent(
      Metrics::CupRpcVerificationEvent::VERIFICATION_SUCCEEDED);
  return serialized_response;
}

client_update_protocol::Ecdsa& CUPImpl::GetQuerySigner() {
  return *query_signer_.get();
}

}  // namespace cup

}  // namespace autofill_assistant
