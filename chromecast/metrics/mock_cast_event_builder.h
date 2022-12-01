// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_METRICS_MOCK_CAST_EVENT_BUILDER_H_
#define CHROMECAST_METRICS_MOCK_CAST_EVENT_BUILDER_H_

#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/time/time.h"
#include "chromecast/metrics/cast_event_builder.h"
#include "net/base/ip_address.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {

class MockCastEventBuilder : public CastEventBuilder {
 public:
  MockCastEventBuilder();
  MockCastEventBuilder(const MockCastEventBuilder&) = delete;
  MockCastEventBuilder& operator=(const MockCastEventBuilder&) = delete;
  ~MockCastEventBuilder() override;
  MOCK_METHOD(std::string, GetName, (), (override));
  MOCK_METHOD(CastEventBuilder&, SetName, (const std::string&), (override));
  MOCK_METHOD(CastEventBuilder&, SetTime, (const base::TimeTicks&), (override));
  MOCK_METHOD(CastEventBuilder&,
              SetTimezoneId,
              (const std::string&),
              (override));
  MOCK_METHOD(CastEventBuilder&, SetAppId, (const std::string&), (override));
  MOCK_METHOD(CastEventBuilder&,
              SetRemoteAppId,
              (const std::string&),
              (override));
  MOCK_METHOD(CastEventBuilder&,
              SetSessionId,
              (const std::string&),
              (override));
  MOCK_METHOD(CastEventBuilder&,
              SetSdkVersion,
              (const std::string&),
              (override));
  MOCK_METHOD(CastEventBuilder&,
              SetMplVersion,
              (const std::string&),
              (override));
  MOCK_METHOD(CastEventBuilder&,
              SetConnectionInfo,
              (const std::string&, const std::string&),
              (override));
  MOCK_METHOD(CastEventBuilder&,
              SetGroupUuid,
              (const std::string&),
              (override));
  MOCK_METHOD(CastEventBuilder&, SetExtraValue, (int64_t), (override));
  MOCK_METHOD(CastEventBuilder&,
              SetConversationKey,
              (const std::string&),
              (override));
  MOCK_METHOD(CastEventBuilder&, SetRequestId, (int32_t), (override));
  MOCK_METHOD(CastEventBuilder&, SetEventId, (const std::string&), (override));
  MOCK_METHOD(CastEventBuilder&,
              SetAoghRequestId,
              (const std::string&),
              (override));
  MOCK_METHOD(CastEventBuilder&, SetAoghLocalDeviceId, (int64_t), (override));
  MOCK_METHOD(CastEventBuilder&,
              SetAoghAgentId,
              (const std::string&),
              (override));
  MOCK_METHOD(CastEventBuilder&,
              SetAoghStandardAgentId,
              (const std::string&),
              (override));
  MOCK_METHOD(CastEventBuilder&,
              SetUiVersion,
              (const std::string&),
              (override));
  MOCK_METHOD(CastEventBuilder&,
              SetAuditReport,
              (const std::string&),
              (override));
  MOCK_METHOD(CastEventBuilder&, SetDuoCoreVersion, (int64_t), (override));
  MOCK_METHOD(CastEventBuilder&,
              SetHotwordModelId,
              (const std::string&),
              (override));
  MOCK_METHOD(CastEventBuilder&,
              SetDiscoveryAppSubtype,
              (const std::string&),
              (override));
  MOCK_METHOD(CastEventBuilder&,
              SetDiscoveryNamespaceSubtype,
              (const std::string&),
              (override));
  MOCK_METHOD(CastEventBuilder&,
              SetDiscoverySender,
              (const net::IPAddressBytes&),
              (override));
  MOCK_METHOD(CastEventBuilder&, SetDiscoveryUnicastFlag, (bool), (override));
  MOCK_METHOD(CastEventBuilder&,
              SetFeatureVector,
              (const std::vector<float>&),
              (override));
  MOCK_METHOD(CastEventBuilder&,
              AddMetadata,
              (const std::string&, int64_t),
              (override));
  MOCK_METHOD(CastEventBuilder&, SetLaunchFrom, (LaunchFrom), (override));

  MOCK_METHOD(CastEventBuilder&,
              MergeFrom,
              (const ::metrics::CastLogsProto_CastEventProto*),
              (override));
  MOCK_METHOD(::metrics::CastLogsProto_CastEventProto*, Build, (), (override));
};

// Stores the last value of each field that was set in the builder. These values
// are exposed as public members for tests to verify.
class FakeCastEventBuilder : public CastEventBuilder {
 public:
  FakeCastEventBuilder();
  FakeCastEventBuilder(const FakeCastEventBuilder&) = delete;
  FakeCastEventBuilder& operator=(const FakeCastEventBuilder&) = delete;
  ~FakeCastEventBuilder() override;

  std::string GetName() override;
  CastEventBuilder& SetName(const std::string& arg_name) override;
  CastEventBuilder& SetTime(const base::TimeTicks& arg_time) override;
  CastEventBuilder& SetTimezoneId(const std::string& arg_timezone_id) override;
  CastEventBuilder& SetAppId(const std::string& arg_app_id) override;
  CastEventBuilder& SetRemoteAppId(
      const std::string& arg_remote_app_id) override;
  CastEventBuilder& SetSessionId(const std::string& arg_session_id) override;
  CastEventBuilder& SetSdkVersion(const std::string& arg_sdk_version) override;
  CastEventBuilder& SetMplVersion(const std::string& arg_mpl_version) override;
  CastEventBuilder& SetConnectionInfo(
      const std::string& arg_transport_connection_id,
      const std::string& arg_virtual_connection_id) override;
  CastEventBuilder& SetGroupUuid(const std::string& arg_group_uuid) override;
  CastEventBuilder& SetExtraValue(int64_t arg_extra_value) override;
  CastEventBuilder& SetConversationKey(
      const std::string& arg_conversation_key) override;
  CastEventBuilder& SetRequestId(int32_t request_id) override;
  CastEventBuilder& SetEventId(const std::string& id) override;
  CastEventBuilder& SetAoghRequestId(const std::string& request_id) override;
  CastEventBuilder& SetAoghLocalDeviceId(int64_t local_id) override;
  CastEventBuilder& SetAoghAgentId(const std::string& request_id) override;
  CastEventBuilder& SetAoghStandardAgentId(
      const std::string& standard_agent_id) override;
  CastEventBuilder& SetUiVersion(const std::string& ui_version) override;
  CastEventBuilder& SetAuditReport(const std::string& audit_report) override;
  CastEventBuilder& SetDuoCoreVersion(int64_t version) override;
  CastEventBuilder& SetHotwordModelId(const std::string& model_id) override;
  CastEventBuilder& SetDiscoveryAppSubtype(
      const std::string& arg_discovery_app_subtype) override;
  CastEventBuilder& SetDiscoveryNamespaceSubtype(
      const std::string& arg_discovery_namespace_subtype) override;
  CastEventBuilder& SetDiscoverySender(
      const net::IPAddressBytes& arg_discovery_sender_bytes) override;
  CastEventBuilder& SetDiscoveryUnicastFlag(
      bool arg_discovery_unicast_flag) override;
  CastEventBuilder& SetFeatureVector(
      const std::vector<float>& arg_features) override;
  CastEventBuilder& AddMetadata(const std::string& name,
                                int64_t value) override;
  CastEventBuilder& SetLaunchFrom(LaunchFrom launch_from) override;
  CastEventBuilder& MergeFrom(
      const ::metrics::CastLogsProto_CastEventProto* event_proto) override;
  ::metrics::CastLogsProto_CastEventProto* Build() override;

  std::string name;
  base::TimeTicks time;
  std::string timezone_id;
  std::string app_id;
  std::string remote_app_id;
  std::string session_id;
  std::string sdk_version;
  std::string mpl_version;
  std::string transport_connection_id;
  std::string virtual_connection_id;
  std::string group_uuid;
  int64_t extra_value;
  std::string conversation_key;
  int32_t request_id;
  std::string event_id;
  std::string aogh_request_id;
  int64_t aogh_local_device_id;
  std::string aogh_agent_id;
  std::string aogh_standard_agent_id;
  std::string ui_version;
  std::string audit_report;
  int64_t duo_core_version;
  std::string hotword_model_id;
  std::string discovery_app_subtype;
  std::string discovery_namespace_subtype;
  net::IPAddressBytes discovery_sender_bytes;
  bool discovery_unicast_flag;
  std::vector<float> features;
  struct Metadata {
    std::string name;
    int64_t value;
  };
  Metadata metadata;
  LaunchFrom launch_from = FROM_UNKNOWN;
  const ::metrics::CastLogsProto_CastEventProto* cast_event_proto;
};

}  // namespace chromecast

#endif  // CHROMECAST_INTERNAL_METRICS_MOCK_CAST_EVENT_BUILDER_H_
