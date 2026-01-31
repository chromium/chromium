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

base::DictValue PolicyReferenceToReadableDictionary(
    const PolicyReference& policy);
std::ostream& operator<<(std::ostream& stream, const PolicyReference& policy);

base::DictValue InvokeNextToReadableDictionary(const InvokeNext& invoke_next);
std::ostream& operator<<(std::ostream& stream, const InvokeNext& invoke_next);

base::DictValue ClientDirectiveToReadableDictionary(
    const ClientDirective& directive);
std::ostream& operator<<(std::ostream& stream,
                         const ClientDirective& directive);

base::DictValue DeviceMetadataPacketToReadableDictionary(
    const DeviceMetadataPacket& packet);
std::ostream& operator<<(std::ostream& stream,
                         const DeviceMetadataPacket& packet);

base::DictValue EncryptedGroupPrivateKeyToReadableDictionary(
    const EncryptedGroupPrivateKey& key);
std::ostream& operator<<(std::ostream& stream,
                         const EncryptedGroupPrivateKey& key);

base::DictValue SyncMetadataResponseToReadableDictionary(
    const SyncMetadataResponse& response);
std::ostream& operator<<(std::ostream& stream,
                         const SyncMetadataResponse& response);

base::DictValue FeatureStatusToReadableDictionary(
    const DeviceFeatureStatus::FeatureStatus& status);
std::ostream& operator<<(std::ostream& stream,
                         const DeviceFeatureStatus::FeatureStatus& status);

base::DictValue DeviceFeatureStatusToReadableDictionary(
    const DeviceFeatureStatus& status);
std::ostream& operator<<(std::ostream& stream,
                         const DeviceFeatureStatus& status);

base::DictValue BatchGetFeatureStatusesResponseToReadableDictionary(
    const BatchGetFeatureStatusesResponse& response);
std::ostream& operator<<(std::ostream& stream,
                         const BatchGetFeatureStatusesResponse& response);

base::DictValue DeviceActivityStatusToReadableDictionary(
    const DeviceActivityStatus& status);
std::ostream& operator<<(std::ostream& stream,
                         const DeviceActivityStatus& status);

base::DictValue GetDevicesActivityStatusResponseToReadableDictionary(
    const GetDevicesActivityStatusResponse& response);
std::ostream& operator<<(std::ostream& stream,
                         const GetDevicesActivityStatusResponse& response);

base::DictValue BeaconSeedToReadableDictionary(const BeaconSeed& seed);
std::ostream& operator<<(std::ostream& stream, const BeaconSeed& seed);

base::DictValue BetterTogetherDeviceMetadataToReadableDictionary(
    const BetterTogetherDeviceMetadata& metadata);
std::ostream& operator<<(std::ostream& stream,
                         const BetterTogetherDeviceMetadata& metadata);

}  // namespace cryptauthv2

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PROTO_CRYPTAUTH_LOGGING_H_
