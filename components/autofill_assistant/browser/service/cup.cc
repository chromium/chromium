// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cup.h"

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/client_update_protocol/ecdsa.h"

namespace {

// This is an ECDSA prime256v1 named-curve key.
constexpr int kKeyVersion = 11;
constexpr char kKeyPubBytesBase64[] =
    "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEgH30WRJf4g6I2C1FKsBQF3qHANLw"
    "thwYsNt2PWTDQBS0ufSRE83piOPoJQcePzTkMfbghjnZerDjLJhBsDkfFg==";

std::string GetKey(const char* key_bytes_base64) {
  std::string result;
  return base::Base64Decode(std::string(key_bytes_base64), &result)
             ? result
             : std::string();
}

bool ShouldSignGetActionsRequests() {
  return base::FeatureList::IsEnabled(
      autofill_assistant::features::kAutofillAssistantSignGetActionsRequests);
}

bool ShouldVerifyGetActionsResponses() {
  return ShouldSignGetActionsRequests() &&
         base::FeatureList::IsEnabled(
             autofill_assistant::features::
                 kAutofillAssistantVerifyGetActionsResponses);
}

}  // namespace

namespace autofill_assistant {

std::unique_ptr<client_update_protocol::Ecdsa> CUP::CreateQuerySigner() {
  return client_update_protocol::Ecdsa::Create(kKeyVersion,
                                               GetKey(kKeyPubBytesBase64));
}

bool CUP::ShouldSignRequests(RpcType rpc_type) {
  return ShouldSignGetActionsRequests() && rpc_type == RpcType::GET_ACTIONS;
}

bool CUP::ShouldVerifyResponses(RpcType rpc_type) {
  return ShouldVerifyGetActionsResponses() && rpc_type == RpcType::GET_ACTIONS;
}

CUP::CUP(std::unique_ptr<client_update_protocol::Ecdsa> query_signer)
    : query_signer_{std::move(query_signer)} {
  DCHECK(query_signer_);
}

CUP::~CUP() = default;

std::string CUP::PackAndSignRequest(const std::string& original_request) {
  return PackGetActionsRequest(original_request);
}

absl::optional<std::string> CUP::UnpackResponse(
    const std::string& original_response) {
  return UnpackGetActionsResponse(original_response);
}

std::string CUP::PackGetActionsRequest(const std::string& original_request) {
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

absl::optional<std::string> CUP::UnpackGetActionsResponse(
    const std::string& original_response) {
  autofill_assistant::ActionsResponseProto actions_response;
  if (!actions_response.ParseFromString(original_response)) {
    LOG(ERROR) << "Failed to parse server response";
    return absl::nullopt;
  }

  std::string serialized_response = actions_response.cup_data().response();
  if (!query_signer_->ValidateResponse(
          serialized_response, actions_response.cup_data().ecdsa_signature())) {
    LOG(ERROR) << "CUP RPC response verification failed";
    return absl::nullopt;
  }

  return serialized_response;
}

client_update_protocol::Ecdsa& CUP::GetQuerySigner() {
  return *query_signer_.get();
}

}  // namespace autofill_assistant
