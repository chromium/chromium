// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/metrics/cast_event_builder_impl.h"

#include "base/logging.h"
#include "base/metrics/metrics_hashes.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "chromecast/base/hash_util.h"
#include "chromecast/metrics/cast_event_builder.h"
#include "chromecast/metrics/metrics_util.h"
#include "third_party/metrics_proto/cast_logs.pb.h"

namespace chromecast {

namespace {
// Bitmasks and values for the |transport_connection_id| field when recording
// discovery metrics for MDNS.
const uint32_t kDiscoverySenderMask = 0x0000FFFF;
const uint32_t kDiscoveryUnicastBit = 0x80000000;
}  // namespace

CastEventBuilderImpl::CastEventBuilderImpl()
    : event_proto_(new ::metrics::CastLogsProto_CastEventProto) {
  event_proto_->set_time_msec(
      (base::TimeTicks::Now() - base::TimeTicks()).InMilliseconds());
}

CastEventBuilderImpl::~CastEventBuilderImpl() {}

std::string CastEventBuilderImpl::GetName() {
  return unhashed_event_name_;
}

CastEventBuilder& CastEventBuilderImpl::SetName(const std::string& name) {
  unhashed_event_name_ = name;
  event_proto_->set_name_hash(base::HashMetricName(name));
  DVLOG(2) << "Hash metric " << name << " = " << std::hex
           << event_proto_->name_hash();
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::SetTime(const base::TimeTicks& time) {
  event_proto_->set_time_msec((time - base::TimeTicks()).InMilliseconds());
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::SetTimezoneId(
    const std::string& timezone_id) {
  event_proto_->set_timezone_id(timezone_id);
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::SetAppId(const std::string& app_id) {
  event_proto_->set_app_id(HashAppId32(app_id));
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::SetRemoteAppId(
    const std::string& remote_app_id) {
  event_proto_->set_remote_app_id(HashAppId32(remote_app_id));
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::SetSessionId(
    const std::string& session_id) {
  event_proto_->set_application_session_id(HashSessionId64(session_id));
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::SetSdkVersion(
    const std::string& sdk_version) {
  event_proto_->set_cast_receiver_version(HashSdkVersion64(sdk_version));
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::SetMplVersion(
    const std::string& mpl_version) {
  event_proto_->set_cast_mpl_version(HashSdkVersion64(mpl_version));
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::SetConnectionInfo(
    const std::string& transport_connection_id,
    const std::string& virtual_connection_id) {
  event_proto_->set_transport_connection_id(
      HashSocketId32(transport_connection_id));
  event_proto_->set_virtual_connection_id(
      HashConnectionId32(virtual_connection_id));
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::SetGroupUuid(
    const std::string& group_uuid) {
  event_proto_->set_group_uuid(base::HashMetricName(group_uuid));
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::SetExtraValue(int64_t extra_value) {
  event_proto_->set_value(extra_value);
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::SetConversationKey(
    const std::string& conversation_key) {
  event_proto_->set_conversation_key(conversation_key);
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::SetRequestId(int32_t request_id) {
  event_proto_->set_request_id(request_id);
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::SetEventId(
    const std::string& event_id) {
  event_proto_->set_event_id(event_id);
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::SetAoghRequestId(
    const std::string& request_id) {
  event_proto_->set_aogh_request_id(request_id);
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::SetAoghLocalDeviceId(int64_t local_id) {
  event_proto_->set_aogh_local_device_id(local_id);
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::SetAoghAgentId(
    const std::string& agent_id) {
  event_proto_->set_aogh_agent_id(agent_id);
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::SetAoghStandardAgentId(
    const std::string& standard_agent_id) {
  event_proto_->set_aogh_standard_agent_id(standard_agent_id);
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::SetUiVersion(const std::string& value) {
  event_proto_->set_ui_version(value);
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::SetAuditReport(
    const std::string& audit_report) {
  event_proto_->set_selinux_audit_detail(audit_report);
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::SetDuoCoreVersion(int64_t version) {
  event_proto_->set_duo_core_version(version);
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::SetHotwordModelId(
    const std::string& model_id) {
  event_proto_->set_hotword_model_id(model_id);
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::SetDiscoveryAppSubtype(
    const std::string& app_id) {
  // Application subtype can be directly added in full.
  event_proto_->set_app_id(HashAppId32(app_id));
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::SetDiscoveryNamespaceSubtype(
    const std::string& namespace_hash) {
  // Namespace hash is a SHA-1 string that is 160-bits (20 bytes) of
  // information, so a 40 character long hex string. We only have 32-bits to
  // upload, so pull out the first 8 characters and hex decode it like an app-
  // id.
  event_proto_->set_app_id(HashAppId32(namespace_hash.substr(0, 8)));
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::SetDiscoverySender(
    const net::IPAddressBytes& sender_ip) {
  // Pack the last two bytes of the sender IP address into a fragment that takes
  // up the final two bytes.
  uint32_t sender_fragment = GetIPAddressFragmentForLogging(sender_ip);

  // Re-use the |transport_connection_id| field to store sender IP address info,
  // since discovery messages will not use this field normally. This gives
  // 32-bits of space to store sender IP info, although only the lower 16-bits
  // are used.
  //
  // Preserve the existing fields by only clearing the sender address (lower
  // 16-bits), then OR with new sender fragment.
  uint32_t value = event_proto_->transport_connection_id();
  value &= ~kDiscoverySenderMask;
  value |= (sender_fragment & kDiscoverySenderMask);
  event_proto_->set_transport_connection_id(value);
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::SetDiscoveryUnicastFlag(
    bool uses_unicast) {
  // Re-use the highest bit in |transport_connection_id| field to store the
  // unicast status of the discovery event, since discovery messages will not
  // use this field normally. Other data may be stored in bits 0-30, so preserve
  // the existing bits by only modifying the unicast bit.
  uint32_t value = event_proto_->transport_connection_id();
  if (uses_unicast) {
    value |= kDiscoveryUnicastBit;
  } else {
    value &= ~kDiscoveryUnicastBit;
  }
  event_proto_->set_transport_connection_id(value);
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::SetFeatureVector(
    const std::vector<float>& features) {
  event_proto_->mutable_feature_vector()->Resize(features.size(), 0);
  float* mutable_data = event_proto_->mutable_feature_vector()->mutable_data();
  base::ranges::copy(features, mutable_data);
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::AddMetadata(const std::string& name,
                                                    int64_t value) {
  ::metrics::CastLogsProto_CastEventProto_Metadata* const metadata =
      event_proto_->add_metadata();
  metadata->set_name_hash(base::HashMetricName(name));
  metadata->set_value(value);
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::SetLaunchFrom(LaunchFrom launch_from) {
  SetLaunchFromProto(event_proto_.get(), launch_from);
  return *this;
}

CastEventBuilder& CastEventBuilderImpl::MergeFrom(
    const ::metrics::CastLogsProto_CastEventProto* event_proto) {
  if (event_proto) {
    event_proto_->MergeFrom(*event_proto);
  }
  return *this;
}

::metrics::CastLogsProto_CastEventProto* CastEventBuilderImpl::Build() {
  return event_proto_.release();
}

}  // namespace chromecast
