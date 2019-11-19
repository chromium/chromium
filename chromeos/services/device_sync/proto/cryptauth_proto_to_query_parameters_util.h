// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_PROTO_CRYPTAUTH_PROTO_TO_QUERY_PARAMETERS_UTIL_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_PROTO_CRYPTAUTH_PROTO_TO_QUERY_PARAMETERS_UTIL_H_

#include <string>
#include <utility>
#include <vector>

// Utility functions for converting relevant CryptAuth v2 protos to lists of
// key-value pairs to be sent as query parameters in HTTP GET requests. Note: A
// key can have multiple values.
namespace cryptauthv2 {

class BatchGetFeatureStatusesRequest;
class BatchNotifyGroupDevicesRequest;
class GetDevicesActivityStatusRequest;
class ClientMetadata;
class RequestContext;

// Example output with |key_prefix| = "client_metadata.":
//   {
//     {"client_metadata.retry_count", "2"},
//     {"client_metadata.invocation_reason", "13"},
//     {"client_metadata.session_id", "abc123"}
//   }
// Note: |crypto_hardware| field is not processed.
std::vector<std::pair<std::string, std::string>>
ClientMetadataToQueryParameters(const ClientMetadata& client_metadata,
                                const std::string& key_prefix = std::string());

// Example output with |key_prefix| = "context.":
//   {
//     {"context.client_metadata.retry_count", "2"},
//     {"context.client_metadata.invocation_reason", "13"},
//     {"context.client_metadata.session_id", "abc"}
//     {"context.group", "DeviceSync:BetterTogether"},
//     {"context.device_id", "123"},
//     {"context.device_id_token", "123token"},
//   }
std::vector<std::pair<std::string, std::string>>
RequestContextToQueryParameters(const RequestContext& context,
                                const std::string& key_prefix = std::string());

// Example output:
//   {
//     {"context.client_metadata.retry_count", "2"},
//     {"context.client_metadata.invocation_reason", "13"},
//     {"context.client_metadata.session_id", "abc"}
//     {"context.group", "DeviceSync:BetterTogether"},
//     {"context.device_id", "123"},
//     {"context.device_id_token", "123token"},
//     {"notify_device_ids", "123"},
//     {"notify_device_ids", "456"},
//     {"target_service", "2"},
//     {"feature_type", "my_feature"}};
//   }
std::vector<std::pair<std::string, std::string>>
BatchNotifyGroupDevicesRequestToQueryParameters(
    const BatchNotifyGroupDevicesRequest& request);

// Example output:
//   {
//     {"context.client_metadata.retry_count", "2"},
//     {"context.client_metadata.invocation_reason", "13"},
//     {"context.client_metadata.session_id", "abc"}
//     {"context.group", "DeviceSync:BetterTogether"},
//     {"context.device_id", "123"},
//     {"context.device_id_token", "123token"},
//     {"device_ids", "123"},
//     {"device_ids", "456"},
//     {"feature_types", "my_feature_1"},
//     {"feature_types", "my_feature_2"}};
//   }
std::vector<std::pair<std::string, std::string>>
BatchGetFeatureStatusesRequestToQueryParameters(
    const BatchGetFeatureStatusesRequest& request);

// Example output:
//   {
//     {"context.client_metadata.retry_count", "2"},
//     {"context.client_metadata.invocation_reason", "13"},
//     {"context.client_metadata.session_id", "abc"}
//     {"context.group", "DeviceSync:BetterTogether"},
//     {"context.device_id", "123"},
//     {"context.device_id_token", "123token"}};
//   }
std::vector<std::pair<std::string, std::string>>
GetDevicesActivityStatusRequestToQueryParameters(
    const GetDevicesActivityStatusRequest& request);

}  // namespace cryptauthv2

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_PROTO_CRYPTAUTH_PROTO_TO_QUERY_PARAMETERS_UTIL_H_
