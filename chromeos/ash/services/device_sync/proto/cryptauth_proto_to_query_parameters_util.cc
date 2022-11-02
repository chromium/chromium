// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/proto/cryptauth_proto_to_query_parameters_util.h"

#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_devicesync.pb.h"

namespace cryptauthv2 {

namespace {

const char kSubFieldDelimiter[] = ".";

const char kClientMetadataRetryCount[] = "retry_count";
const char kClientMetadataInvocationReason[] = "invocation_reason";
const char kClientMetadataSessionId[] = "session_id";

const char kRequestContextGroup[] = "group";
const char kRequestContextClientMetadata[] = "client_metadata";
const char kRequestContextDeviceId[] = "device_id";
const char kRequestContextDeviceIdToken[] = "device_id_token";

const char kBatchNotifyGroupDevicesRequestContext[] = "context";
const char kBatchNotifyGroupDevicesRequestNotifyDeviceIds[] =
    "notify_device_ids";
const char kBatchNotifyGroupDevicesRequestTargetService[] = "target_service";
const char kBatchNotifyGroupDevicesRequestFeatureType[] = "feature_type";

const char kBatchGetFeatureStatusesRequestContext[] = "context";
const char kBatchGetFeatureStatusesRequestDeviceIds[] = "device_ids";
const char kBatchGetFeatureStatusesRequestFeatureTypes[] = "feature_types";

const char kGetDevicesActivityStatusRequestContext[] = "context";

}  // namespace

std::vector<std::pair<std::string, std::string>>
ClientMetadataToQueryParameters(const ClientMetadata& client_metadata,
                                const std::string& key_prefix) {
  // |crypto_hardware| is not processed; make sure it has no value.
  DCHECK(!client_metadata.has_crypto_hardware());

  return {
      {key_prefix + kClientMetadataRetryCount,
       base::NumberToString(client_metadata.retry_count())},
      {key_prefix + kClientMetadataInvocationReason,
       base::NumberToString(client_metadata.invocation_reason())},
      {key_prefix + kClientMetadataSessionId, client_metadata.session_id()}};
}

std::vector<std::pair<std::string, std::string>>
RequestContextToQueryParameters(const RequestContext& context,
                                const std::string& key_prefix) {
  std::vector<std::pair<std::string, std::string>> pairs =
      ClientMetadataToQueryParameters(
          context.client_metadata(),
          key_prefix + kRequestContextClientMetadata + kSubFieldDelimiter);

  pairs.insert(
      pairs.end(),
      {{key_prefix + kRequestContextGroup, context.group()},
       {key_prefix + kRequestContextDeviceId, context.device_id()},
       {key_prefix + kRequestContextDeviceIdToken, context.device_id_token()}});

  return pairs;
}

std::vector<std::pair<std::string, std::string>>
BatchNotifyGroupDevicesRequestToQueryParameters(
    const BatchNotifyGroupDevicesRequest& request) {
  std::vector<std::pair<std::string, std::string>> pairs =
      RequestContextToQueryParameters(
          request.context(),
          std::string(kBatchNotifyGroupDevicesRequestContext) +
              kSubFieldDelimiter);

  for (const std::string& notify_device_id : request.notify_device_ids()) {
    pairs.emplace_back(kBatchNotifyGroupDevicesRequestNotifyDeviceIds,
                       notify_device_id);
  }

  pairs.emplace_back(kBatchNotifyGroupDevicesRequestTargetService,
                     base::NumberToString(request.target_service()));
  pairs.emplace_back(kBatchNotifyGroupDevicesRequestFeatureType,
                     request.feature_type());

  return pairs;
}

std::vector<std::pair<std::string, std::string>>
BatchGetFeatureStatusesRequestToQueryParameters(
    const BatchGetFeatureStatusesRequest& request) {
  std::vector<std::pair<std::string, std::string>> pairs =
      RequestContextToQueryParameters(
          request.context(),
          std::string(kBatchGetFeatureStatusesRequestContext) +
              kSubFieldDelimiter);

  for (const std::string& device_id : request.device_ids())
    pairs.emplace_back(kBatchGetFeatureStatusesRequestDeviceIds, device_id);

  for (const std::string& feature_type : request.feature_types()) {
    pairs.emplace_back(kBatchGetFeatureStatusesRequestFeatureTypes,
                       feature_type);
  }

  return pairs;
}

std::vector<std::pair<std::string, std::string>>
GetDevicesActivityStatusRequestToQueryParameters(
    const GetDevicesActivityStatusRequest& request) {
  std::vector<std::pair<std::string, std::string>> pairs =
      RequestContextToQueryParameters(
          request.context(),
          std::string(kGetDevicesActivityStatusRequestContext) +
              kSubFieldDelimiter);

  return pairs;
}

}  // namespace cryptauthv2
