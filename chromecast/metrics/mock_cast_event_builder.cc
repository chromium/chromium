// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/metrics/mock_cast_event_builder.h"

using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRef;

namespace chromecast {

MockCastEventBuilder::MockCastEventBuilder() {
  ON_CALL(*this, GetName()).WillByDefault(Return(""));
  ON_CALL(*this, SetName(_)).WillByDefault(ReturnRef(*this));
  ON_CALL(*this, SetTime(_)).WillByDefault(ReturnRef(*this));
  ON_CALL(*this, SetAppId(_)).WillByDefault(ReturnRef(*this));
  ON_CALL(*this, SetRemoteAppId(_)).WillByDefault(ReturnRef(*this));
  ON_CALL(*this, SetSessionId(_)).WillByDefault(ReturnRef(*this));
  ON_CALL(*this, SetSdkVersion(_)).WillByDefault(ReturnRef(*this));
  ON_CALL(*this, SetMplVersion(_)).WillByDefault(ReturnRef(*this));
  ON_CALL(*this, SetConnectionInfo(_, _)).WillByDefault(ReturnRef(*this));
  ON_CALL(*this, SetGroupUuid(_)).WillByDefault(ReturnRef(*this));
  ON_CALL(*this, SetExtraValue(_)).WillByDefault(ReturnRef(*this));
  ON_CALL(*this, SetConversationKey(_)).WillByDefault(ReturnRef(*this));
  ON_CALL(*this, SetRequestId(_)).WillByDefault(ReturnRef(*this));
  ON_CALL(*this, SetEventId(_)).WillByDefault(ReturnRef(*this));
  ON_CALL(*this, SetAoghRequestId(_)).WillByDefault(ReturnRef(*this));
  ON_CALL(*this, SetAoghLocalDeviceId(_)).WillByDefault(ReturnRef(*this));
  ON_CALL(*this, SetAoghAgentId(_)).WillByDefault(ReturnRef(*this));
  ON_CALL(*this, SetAoghStandardAgentId(_)).WillByDefault(ReturnRef(*this));
  ON_CALL(*this, SetUiVersion(_)).WillByDefault(ReturnRef(*this));
  ON_CALL(*this, SetAuditReport(_)).WillByDefault(ReturnRef(*this));
  ON_CALL(*this, SetDuoCoreVersion(_)).WillByDefault(ReturnRef(*this));
  ON_CALL(*this, SetHotwordModelId(_)).WillByDefault(ReturnRef(*this));
  ON_CALL(*this, SetDiscoveryAppSubtype(_)).WillByDefault(ReturnRef(*this));
  ON_CALL(*this, SetDiscoveryNamespaceSubtype(_))
      .WillByDefault(ReturnRef(*this));
  ON_CALL(*this, SetDiscoverySender(_)).WillByDefault(ReturnRef(*this));
  ON_CALL(*this, SetDiscoveryUnicastFlag(_)).WillByDefault(ReturnRef(*this));
  ON_CALL(*this, SetFeatureVector(_)).WillByDefault(ReturnRef(*this));
  ON_CALL(*this, AddMetadata(_, _)).WillByDefault(ReturnRef(*this));
  ON_CALL(*this, MergeFrom(_)).WillByDefault(ReturnRef(*this));
}

MockCastEventBuilder::~MockCastEventBuilder() {}

FakeCastEventBuilder::FakeCastEventBuilder() {}

FakeCastEventBuilder::~FakeCastEventBuilder() {}

std::string FakeCastEventBuilder::GetName() {
  return name;
}

CastEventBuilder& FakeCastEventBuilder::SetName(const std::string& arg_name) {
  name = arg_name;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::SetTime(
    const base::TimeTicks& arg_time) {
  time = arg_time;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::SetTimezoneId(
    const std::string& arg_timezone_id) {
  timezone_id = arg_timezone_id;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::SetAppId(
    const std::string& arg_app_id) {
  app_id = arg_app_id;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::SetRemoteAppId(
    const std::string& arg_remote_app_id) {
  remote_app_id = arg_remote_app_id;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::SetSessionId(
    const std::string& arg_session_id) {
  session_id = arg_session_id;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::SetSdkVersion(
    const std::string& arg_sdk_version) {
  sdk_version = arg_sdk_version;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::SetMplVersion(
    const std::string& arg_mpl_version) {
  mpl_version = arg_mpl_version;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::SetConnectionInfo(
    const std::string& arg_transport_connection_id,
    const std::string& arg_virtual_connection_id) {
  transport_connection_id = arg_transport_connection_id;
  virtual_connection_id = arg_virtual_connection_id;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::SetGroupUuid(
    const std::string& arg_group_uuid) {
  group_uuid = arg_group_uuid;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::SetExtraValue(int64_t arg_extra_value) {
  extra_value = arg_extra_value;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::SetConversationKey(
    const std::string& arg_conversation_key) {
  conversation_key = arg_conversation_key;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::SetRequestId(int32_t arg_request_id) {
  request_id = arg_request_id;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::SetEventId(const std::string& arg_id) {
  event_id = arg_id;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::SetAoghRequestId(
    const std::string& arg_request_id) {
  aogh_request_id = arg_request_id;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::SetAoghLocalDeviceId(
    int64_t arg_local_id) {
  aogh_local_device_id = arg_local_id;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::SetAoghAgentId(
    const std::string& arg_agent_id) {
  aogh_agent_id = arg_agent_id;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::SetAoghStandardAgentId(
    const std::string& standard_agent_id) {
  aogh_standard_agent_id = standard_agent_id;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::SetUiVersion(
    const std::string& arg_ui_version) {
  ui_version = arg_ui_version;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::SetAuditReport(
    const std::string& arg_audit_report) {
  audit_report = arg_audit_report;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::SetDuoCoreVersion(int64_t version) {
  duo_core_version = version;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::SetHotwordModelId(
    const std::string& model_id) {
  hotword_model_id = model_id;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::SetDiscoveryAppSubtype(
    const std::string& arg_discovery_app_subtype) {
  discovery_app_subtype = arg_discovery_app_subtype;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::SetDiscoveryNamespaceSubtype(
    const std::string& arg_discovery_namespace_subtype) {
  discovery_namespace_subtype = arg_discovery_namespace_subtype;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::SetDiscoverySender(
    const net::IPAddressBytes& arg_discovery_sender_bytes) {
  discovery_sender_bytes = arg_discovery_sender_bytes;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::SetDiscoveryUnicastFlag(
    bool arg_discovery_unicast_flag) {
  discovery_unicast_flag = arg_discovery_unicast_flag;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::SetFeatureVector(
    const std::vector<float>& arg_features) {
  features = arg_features;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::AddMetadata(const std::string& arg_name,
                                                    int64_t arg_value) {
  metadata.name = arg_name;
  metadata.value = arg_value;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::SetLaunchFrom(
    LaunchFrom new_launch_from) {
  launch_from = new_launch_from;
  return *this;
}

CastEventBuilder& FakeCastEventBuilder::MergeFrom(
    const ::metrics::CastLogsProto_CastEventProto* event_proto) {
  cast_event_proto = event_proto;
  return *this;
}

::metrics::CastLogsProto_CastEventProto* FakeCastEventBuilder::Build() {
  return nullptr;
}

}  // namespace chromecast
