// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PROTO_CRYPTAUTH_V2_TEST_UTIL_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PROTO_CRYPTAUTH_V2_TEST_UTIL_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "chromeos/ash/services/device_sync/proto/cryptauth_better_together_device_metadata.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_better_together_feature_metadata.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_client_app_metadata.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_devicesync.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_directive.pb.h"

namespace cryptauthv2 {

extern const char kTestDeviceSyncGroupName[];
extern const char kTestGcmRegistrationId[];
extern const char kTestInstanceId[];
extern const char kTestInstanceIdToken[];
extern const char kTestLongDeviceId[];
extern const char kTestNoPiiDeviceName[];
extern const char kTestUserPublicKey[];

// Attributes of test ClientDirective.
extern const int32_t kTestClientDirectiveRetryAttempts;
extern const int64_t kTestClientDirectiveCheckinDelayMillis;
extern const int64_t kTestClientDirectivePolicyReferenceVersion;
extern const int64_t kTestClientDirectiveRetryPeriodMillis;
extern const int64_t kTestClientDirectiveCreateTimeMillis;
extern const char kTestClientDirectivePolicyReferenceName[];

ClientMetadata BuildClientMetadata(
    int32_t retry_count,
    const ClientMetadata::InvocationReason& invocation_reason,
    const std::optional<std::string>& session_id = std::nullopt);

PolicyReference BuildPolicyReference(const std::string& name, int64_t version);

KeyDirective BuildKeyDirective(const PolicyReference& policy_reference,
                               int64_t enroll_time_millis);

RequestContext BuildRequestContext(const std::string& group,
                                   const ClientMetadata& client_metadata,
                                   const std::string& device_id,
                                   const std::string& device_id_token);

DeviceFeatureStatus BuildDeviceFeatureStatus(
    const std::string& device_id,
    const std::vector<std::pair<std::string /* feature_type */,
                                bool /* enabled */>>& feature_statuses);

DeviceActivityStatus BuildDeviceActivityStatus(
    const std::string& device_id,
    int64_t last_activity_time_sec,
    const ConnectivityStatus online_status,
    Timestamp last_update_time);

// The data field is set to "start_|start_time_millis|_end_|end_time_millis|".
BeaconSeed BuildBeaconSeedForTest(int64_t start_time_millis,
                                  int64_t end_time_millis);

const ClientAppMetadata& GetClientAppMetadataForTest();

const ClientDirective& GetClientDirectiveForTest();

const RequestContext& GetRequestContextForTest();

const BetterTogetherDeviceMetadata& GetBetterTogetherDeviceMetadataForTest();

}  // namespace cryptauthv2

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PROTO_CRYPTAUTH_V2_TEST_UTIL_H_
