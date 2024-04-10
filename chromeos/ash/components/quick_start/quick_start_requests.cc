// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/quick_start/quick_start_requests.h"

#include "base/base64.h"
#include "base/values.h"
#include "chromeos/ash/components/quick_start/quick_start_message.h"
#include "chromeos/ash/components/quick_start/quick_start_message_type.h"
#include "chromeos/ash/components/quick_start/types.h"
#include "chromeos/constants/devicetype.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "crypto/sha2.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ash::quick_start::requests {

namespace {

// bootstrapOptions key telling the phone how to handle
// challenge UI in case of fallback.
constexpr char kFlowTypeKey[] = "flowType";
// bootstrapOptions key telling the phone the number of
// accounts are expected to transfer account to the target device.
constexpr char kAccountRequirementKey[] = "accountRequirement";
// bootstrapOptions key telling the deviceName of target device.
constexpr char kDeviceNameKey[] = "deviceName";
// bootstrapOptions key containing object with phone actions after transfer.
constexpr char kPostTransferActionKey[] = "PostTransferAction";
// bootstrapOptions URI key inside the PostTransferAction object.
constexpr char kPostTransferActionURIKey[] = "uri";
// bootstrapOptions PostTransferAction uri value.
constexpr char kPostTransferActionURIValue[] =
    "intent:#Intent;action=com.google.android.gms.quickstart.LANDING_SCREEN;"
    "package=com.google.android.gms;end";

// Device names
constexpr char kChromebook[] = "Chromebook";
constexpr char kChromebase[] = "Chromebase";
constexpr char kChromebox[] = "Chromebox";

// Base64 encoded CBOR bytes containing the Fido command. This will be used
// for GetInfo and GetAssertion.
static constexpr char FIDO_MESSAGE_KEY[] = "fidoMessage";

// Maps to AccountRequirementSingle enum value for Account Requirement field
// meaning that at least one account is required on the phone. The user will
// select the specified account to transfer.
// Enum Source: go/bootstrap-options-account-requirement-single.
constexpr int kAccountRequirementSingle = 2;

// Maps to FlowTypeTargetChallenge enum value for Flow Type field meaning that
// the fallback challenge will happen on the target device.
// Enum Source: go/bootstrap-options-flow-type-target-challenge.
constexpr int kFlowTypeTargetChallenge = 2;

const char kRelyingPartyId[] = "google.com";
const char kOrigin[] = "https://accounts.google.com";

// Maps to CBOR byte labelling FIDO request as GetAssertion.
const uint8_t kAuthenticatorGetAssertionCommand = 0x02;
const char kUserPresenceMapKey[] = "up";
const char kUserVerificationMapKey[] = "uv";

// Maps to CBOR byte labelling FIDO request as GetInfo.
const uint8_t kAuthenticatorGetInfoCommand = 0x04;

// Boolean in WifiCredentialsRequest indicating we should request WiFi
// Credentials
constexpr char kRequestWifiKey[] = "request_wifi";

// Key in WifiCredentialsRequest and NotifySourceOfUpdateMessage including the
// shared secret to resume the connection if a reboot occurs.
constexpr char kSharedSecretKey[] = "shared_secret";

// Key in WifiCredentialsRequest and NotifySourceOfUpdateMessage for the session
// ID
constexpr char kSessionIdKey[] = "SESSION_ID";

// Boolean in NotifySourceOfUpdateMessage indicating target device requires an
// update.
constexpr char kNotifySourceOfUpdateMessageKey[] = "forced_update_required";

// bootstrapOptions key to inform source of the target device type.
constexpr char kDeviceTypeKey[] = "deviceType";

// Device type should map to ChromeOS device type. See:
// http://google3/java/com/google/android/gmscore/integ/client/smartdevice/src/com/google/android/gms/smartdevice/d2d/DeviceType.java;l=57
constexpr int kDeviceTypeChrome = 7;

// BootstrapState maps to values set here:
// http://google3/java/com/google/android/gmscore/integ/modules/smartdevice/src/com/google/android/gms/smartdevice/d2d/data/BootstrapState.java
constexpr int kBootstrapStateCancel = 1;
constexpr int kBootstrapStateComplete = 2;

// Key in BootstrapState message.
constexpr char kBootstrapStateKey[] = "bootstrapState";

std::string GetDeviceName() {
  switch (chromeos::GetDeviceType()) {
    case chromeos::DeviceType::kChromebook:
      return kChromebook;
    case chromeos::DeviceType::kChromebase:
      return kChromebase;
    case chromeos::DeviceType::kChromebox:
      return kChromebox;
    default:
      return kChromebook;
  }
}

}  // namespace

std::unique_ptr<QuickStartMessage> BuildBootstrapOptionsRequest() {
  std::unique_ptr<QuickStartMessage> message =
      std::make_unique<QuickStartMessage>(
          QuickStartMessageType::kBootstrapOptions);
  message->GetPayload()->Set(kAccountRequirementKey, kAccountRequirementSingle);
  message->GetPayload()->Set(kFlowTypeKey, kFlowTypeTargetChallenge);
  message->GetPayload()->Set(kDeviceTypeKey, kDeviceTypeChrome);
  message->GetPayload()->Set(kDeviceNameKey, GetDeviceName());

  // TODO(b/332603236): Remove postTransferAction payload when new device info
  // exchange is implemented.
  base::Value::Dict post_transfer_action;
  post_transfer_action.Set(kPostTransferActionURIKey,
                           kPostTransferActionURIValue);

  message->GetPayload()->Set(kPostTransferActionKey,
                             std::move(post_transfer_action));
  return message;
}

std::unique_ptr<QuickStartMessage> BuildAssertionRequestMessage(
    std::array<uint8_t, crypto::kSHA256Length> client_data_hash) {
  cbor::Value request = GenerateGetAssertionRequest(client_data_hash);
  std::vector<uint8_t> ctap_request_command =
      CBOREncodeGetAssertionRequest(std::move(request));

  std::unique_ptr<QuickStartMessage> message =
      std::make_unique<QuickStartMessage>(
          QuickStartMessageType::kSecondDeviceAuthPayload);

  message->GetPayload()->Set(FIDO_MESSAGE_KEY,
                             base::Base64Encode(ctap_request_command));
  return message;
}

std::unique_ptr<QuickStartMessage> BuildGetInfoRequestMessage() {
  std::vector<uint8_t> ctap_request_command({kAuthenticatorGetInfoCommand});
  std::unique_ptr<QuickStartMessage> message =
      std::make_unique<QuickStartMessage>(
          QuickStartMessageType::kSecondDeviceAuthPayload);
  message->GetPayload()->Set(FIDO_MESSAGE_KEY,
                             base::Base64Encode(ctap_request_command));
  return message;
}

std::unique_ptr<QuickStartMessage> BuildRequestWifiCredentialsMessage(
    uint64_t session_id,
    std::string& shared_secret) {
  std::unique_ptr<QuickStartMessage> message =
      std::make_unique<QuickStartMessage>(
          QuickStartMessageType::kQuickStartPayload);
  message->GetPayload()->Set(kRequestWifiKey, true);
  std::string shared_secret_str(shared_secret.begin(), shared_secret.end());
  std::string shared_secret_base64 = base::Base64Encode(shared_secret_str);
  message->GetPayload()->Set(kSharedSecretKey, shared_secret_base64);
  message->GetPayload()->Set(kSessionIdKey, base::NumberToString(session_id));
  return message;
}

cbor::Value GenerateGetAssertionRequest(
    std::array<uint8_t, crypto::kSHA256Length> client_data_hash) {
  url::Origin origin = url::Origin::Create(GURL(kOrigin));
  cbor::Value::MapValue cbor_map;
  cbor_map.insert_or_assign(cbor::Value(0x01), cbor::Value(kRelyingPartyId));
  cbor_map.insert_or_assign(cbor::Value(0x02), cbor::Value(client_data_hash));
  cbor::Value::MapValue option_map;
  option_map.insert_or_assign(cbor::Value(kUserPresenceMapKey),
                              cbor::Value(true));
  option_map.insert_or_assign(cbor::Value(kUserVerificationMapKey),
                              cbor::Value(true));
  cbor_map.insert_or_assign(cbor::Value(0x05),
                            cbor::Value(std::move(option_map)));
  return cbor::Value(std::move(cbor_map));
}

std::vector<uint8_t> CBOREncodeGetAssertionRequest(const cbor::Value& request) {
  // Encode the CtapGetAssertionRequest into cbor bytes vector.
  std::optional<std::vector<uint8_t>> cbor_bytes = cbor::Writer::Write(request);
  CHECK(cbor_bytes);
  std::vector<uint8_t> request_bytes = std::move(*cbor_bytes);
  // Add the command byte to the beginning of this now fully encoded cbor bytes
  // vector.
  request_bytes.insert(request_bytes.begin(),
                       kAuthenticatorGetAssertionCommand);
  return request_bytes;
}

std::unique_ptr<QuickStartMessage> BuildNotifySourceOfUpdateMessage(
    uint64_t session_id,
    const base::span<uint8_t, 32> shared_secret) {
  std::unique_ptr<QuickStartMessage> message =
      std::make_unique<QuickStartMessage>(
          QuickStartMessageType::kQuickStartPayload);
  message->GetPayload()->Set(kNotifySourceOfUpdateMessageKey, true);

  std::string shared_secret_str(shared_secret.begin(), shared_secret.end());
  std::string shared_secret_base64 = base::Base64Encode(shared_secret_str);
  message->GetPayload()->Set(kSharedSecretKey, shared_secret_base64);
  message->GetPayload()->Set(kSessionIdKey, base::NumberToString(session_id));

  return message;
}

std::unique_ptr<QuickStartMessage> BuildBootstrapStateCancelMessage() {
  std::unique_ptr<QuickStartMessage> message =
      std::make_unique<QuickStartMessage>(
          QuickStartMessageType::kBootstrapState);
  message->GetPayload()->Set(kBootstrapStateKey, kBootstrapStateCancel);
  return message;
}

std::unique_ptr<QuickStartMessage> BuildBootstrapStateCompleteMessage() {
  std::unique_ptr<QuickStartMessage> message =
      std::make_unique<QuickStartMessage>(
          QuickStartMessageType::kBootstrapState);
  message->GetPayload()->Set(kBootstrapStateKey, kBootstrapStateComplete);

  // TODO(b/332603236): Remove postTransferAction payload when new device info
  // exchange is implemented.
  base::Value::Dict post_transfer_action;
  post_transfer_action.Set(kPostTransferActionURIKey,
                           kPostTransferActionURIValue);
  message->GetPayload()->Set(kPostTransferActionKey,
                             std::move(post_transfer_action));
  return message;
}

}  // namespace ash::quick_start::requests
