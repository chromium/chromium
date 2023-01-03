// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PROTO_CRYPTAUTH_LOGGING_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PROTO_CRYPTAUTH_LOGGING_H_

#include <ostream>
#include <string>

#include "base/values.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_better_together_device_metadata.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_devicesync.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_directive.pb.h"

namespace cryptauthv2 {

std::string TruncateStringForLogs(const std::string& str);

std::string TargetServiceToString(TargetService service);
std::ostream& operator<<(std::ostream& stream, const TargetService& service);

std::string InvocationReasonToString(ClientMetadata::InvocationReason reason);
std::ostream& operator<<(std::ostream& stream,
                         const ClientMetadata::InvocationReason& reason);

std::string ConnectivityStatusToString(ConnectivityStatus status);
std::ostream& operator<<(std::ostream& stream,
                         const ConnectivityStatus& status);

base::Value::Dict PolicyReferenceToReadableDictionary(
    const PolicyReference& policy);
std::ostream& operator<<(std::ostream& stream, const PolicyReference& policy);

base::Value::Dict InvokeNextToReadableDictionary(const InvokeNext& invoke_next);
std::ostream& operator<<(std::ostream& stream, const InvokeNext& invoke_next);

base::Value::Dict ClientDirectiveToReadableDictionary(
    const ClientDirective& directive);
std::ostream& operator<<(std::ostream& stream,
                         const ClientDirective& directive);

base::Value::Dict DeviceMetadataPacketToReadableDictionary(
    const DeviceMetadataPacket& packet);
std::ostream& operator<<(std::ostream& stream,
                         const DeviceMetadataPacket& packet);

base::Value::Dict EncryptedGroupPrivateKeyToReadableDictionary(
    const EncryptedGroupPrivateKey& key);
std::ostream& operator<<(std::ostream& stream,
                         const EncryptedGroupPrivateKey& key);

base::Value::Dict SyncMetadataResponseToReadableDictionary(
    const SyncMetadataResponse& response);
std::ostream& operator<<(std::ostream& stream,
                         const SyncMetadataResponse& response);

base::Value::Dict FeatureStatusToReadableDictionary(
    const DeviceFeatureStatus::FeatureStatus& status);
std::ostream& operator<<(std::ostream& stream,
                         const DeviceFeatureStatus::FeatureStatus& status);

base::Value::Dict DeviceFeatureStatusToReadableDictionary(
    const DeviceFeatureStatus& status);
std::ostream& operator<<(std::ostream& stream,
                         const DeviceFeatureStatus& status);

base::Value::Dict BatchGetFeatureStatusesResponseToReadableDictionary(
    const BatchGetFeatureStatusesResponse& response);
std::ostream& operator<<(std::ostream& stream,
                         const BatchGetFeatureStatusesResponse& response);

base::Value::Dict DeviceActivityStatusToReadableDictionary(
    const DeviceActivityStatus& status);
std::ostream& operator<<(std::ostream& stream,
                         const DeviceActivityStatus& status);

base::Value::Dict GetDevicesActivityStatusResponseToReadableDictionary(
    const GetDevicesActivityStatusResponse& response);
std::ostream& operator<<(std::ostream& stream,
                         const GetDevicesActivityStatusResponse& response);

base::Value::Dict BeaconSeedToReadableDictionary(const BeaconSeed& seed);
std::ostream& operator<<(std::ostream& stream, const BeaconSeed& seed);

base::Value::Dict BetterTogetherDeviceMetadataToReadableDictionary(
    const BetterTogetherDeviceMetadata& metadata);
std::ostream& operator<<(std::ostream& stream,
                         const BetterTogetherDeviceMetadata& metadata);

}  // namespace cryptauthv2

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PROTO_CRYPTAUTH_LOGGING_H_
