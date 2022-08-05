// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/oobe_quick_start/connectivity/target_fido_controller.h"

#include "base/json/json_writer.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/values.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "crypto/sha2.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ash::quick_start {

namespace {

const char kRelyingPartyId[] = "google.com";
const char kOrigin[] = "https://accounts.google.com";
const char kCtapRequestType[] = "webauthn.get";

const uint8_t kAuthenticatorGetAssertionCommand = 0x02;
const char kUserPresenceMapKey[] = "up";
const char kUserVerificationMapKey[] = "uv";

}  // namespace

TargetFidoController::TargetFidoController(
    NearbyConnectionsManager* nearby_connections_manager)
    : nearby_connections_manager_(nearby_connections_manager) {
  // TODO(b/234655072): Uncomment the following line after
  // NearbyConnectionsManager defined.

  // CHECK(nearby_connections_manager_);
}

TargetFidoController::~TargetFidoController() = default;

void TargetFidoController::RequestAssertion(const std::string& challenge_b64url,
                                            ResultCallback callback) {
  if (challenge_b64url.empty()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }
  cbor::Value request = GenerateGetAssertionRequest(challenge_b64url);
  std::vector<uint8_t> ctap_request_command =
      CBOREncodeGetAssertionRequest(std::move(request));

  // TODO(b/234655072): This is a stub and not real logic. Add the actual logic.
  std::move(callback).Run(/*success=*/true);
}

// GenerateGetAssertionRequest will take challenge bytes and create an instance
// of cbor::Value of the GetAssertionRequest which can then be CBOR encoded.
cbor::Value TargetFidoController::GenerateGetAssertionRequest(
    const std::string& challenge_b64url) {
  url::Origin origin = url::Origin::Create(GURL(kOrigin));
  std::string client_data_json = CreateClientDataJson(origin, challenge_b64url);
  cbor::Value::MapValue cbor_map;
  cbor_map[cbor::Value(1)] = cbor::Value(kRelyingPartyId);
  std::array<uint8_t, crypto::kSHA256Length> client_data_hash;
  crypto::SHA256HashString(client_data_json, client_data_hash.data(),
                           client_data_hash.size());
  cbor_map[cbor::Value(2)] = cbor::Value(client_data_hash);
  cbor::Value::MapValue option_map;
  option_map[cbor::Value(kUserPresenceMapKey)] = cbor::Value(true);
  option_map[cbor::Value(kUserVerificationMapKey)] = cbor::Value(true);
  cbor_map[cbor::Value(5)] = cbor::Value(std::move(option_map));
  return cbor::Value(std::move(cbor_map));
}

// CBOREncodeGetAssertionRequest will take a CtapGetAssertionRequest struct
// and encode it into CBOR encoded bytes that can be understood by a FIDO
// authenticator.
std::vector<uint8_t> TargetFidoController::CBOREncodeGetAssertionRequest(
    const cbor::Value& request) {
  // Encode the CtapGetAssertionRequest into cbor bytes vector.
  absl::optional<std::vector<uint8_t>> cbor_bytes =
      cbor::Writer::Write(request);
  DCHECK(cbor_bytes);
  std::vector<uint8_t> request_bytes = std::move(*cbor_bytes);
  // Add the command byte to the beginning of this now fully encoded cbor bytes
  // vector.
  request_bytes.insert(request_bytes.begin(),
                       kAuthenticatorGetAssertionCommand);
  return request_bytes;
}

// This JSON encoding does not follow the strict requirements of the spec[1],
// but that's ok because the validator doesn't demand that. [1]
// https://www.w3.org/TR/webauthn-2/#clientdatajson-serialization
std::string TargetFidoController::CreateClientDataJson(
    const url::Origin& origin,
    const std::string& challenge_b64url) {
  base::Value::Dict collected_client_data;
  collected_client_data.Set("type", kCtapRequestType);
  collected_client_data.Set("challenge", challenge_b64url);
  collected_client_data.Set("origin", origin.Serialize());
  collected_client_data.Set("crossOrigin", false);
  std::string client_data_json;
  base::JSONWriter::Write(collected_client_data, &client_data_json);
  return client_data_json;
}

}  // namespace ash::quick_start
