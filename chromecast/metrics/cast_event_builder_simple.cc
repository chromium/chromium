// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/metrics/cast_event_builder_simple.h"

#include "base/time/time.h"
#include "third_party/metrics_proto/cast_logs.pb.h"

namespace chromecast {

CastEventBuilderSimple::CastEventBuilderSimple()
    : event_proto_(new ::metrics::CastLogsProto_CastEventProto) {
  event_proto_->set_time_msec(
      (base::TimeTicks::Now() - base::TimeTicks()).InMilliseconds());
}

CastEventBuilderSimple::~CastEventBuilderSimple() {}

std::string CastEventBuilderSimple::GetName() {
  return "";
}

CastEventBuilder& CastEventBuilderSimple::SetName(const std::string& name) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::SetTime(const base::TimeTicks& time) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::SetTimezoneId(
    const std::string& timezone_id) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::SetAppId(const std::string& app_id) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::SetRemoteAppId(
    const std::string& remote_app_id) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::SetSessionId(
    const std::string& session_id) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::SetSdkVersion(
    const std::string& sdk_version) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::SetMplVersion(
    const std::string& mpl_version) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::SetConnectionInfo(
    const std::string& transport_connection_id,
    const std::string& virtual_connection_id) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::SetGroupUuid(
    const std::string& group_uuid) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::SetExtraValue(int64_t extra_value) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::SetConversationKey(
    const std::string& conversation_key) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::SetRequestId(int32_t request_id) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::SetEventId(
    const std::string& event_id) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::SetAoghRequestId(
    const std::string& request_id) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::SetAoghLocalDeviceId(
    int64_t local_id) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::SetAoghAgentId(
    const std::string& agent_id) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::SetAoghStandardAgentId(
    const std::string& standard_agent_id) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::SetUiVersion(
    const std::string& value) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::SetAuditReport(
    const std::string& audit_report) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::SetDuoCoreVersion(int64_t version) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::SetHotwordModelId(
    const std::string& model_id) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::SetDiscoveryAppSubtype(
    const std::string& app_id) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::SetDiscoveryNamespaceSubtype(
    const std::string& namespace_hash) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::SetDiscoverySender(
    const net::IPAddressBytes& sender_ip) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::SetDiscoveryUnicastFlag(
    bool uses_unicast) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::SetFeatureVector(
    const std::vector<float>& features) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::AddMetadata(const std::string& name,
                                                      int64_t value) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::SetLaunchFrom(
    LaunchFrom launch_from) {
  return *this;
}

CastEventBuilder& CastEventBuilderSimple::MergeFrom(
    const ::metrics::CastLogsProto_CastEventProto* event_proto) {
  return *this;
}

::metrics::CastLogsProto_CastEventProto* CastEventBuilderSimple::Build() {
  return event_proto_.release();
}

}  // namespace chromecast
