// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_METRICS_CAST_EVENT_BUILDER_IMPL_H_
#define CHROMECAST_METRICS_CAST_EVENT_BUILDER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "chromecast/metrics/cast_event_builder.h"

namespace chromecast {

class CastEventBuilderImpl : public CastEventBuilder {
 public:
  CastEventBuilderImpl();
  CastEventBuilderImpl(const CastEventBuilderImpl&) = delete;
  CastEventBuilderImpl& operator=(const CastEventBuilderImpl&) = delete;
  ~CastEventBuilderImpl() override;

  // CastEventBuilder implementation
  std::string GetName() override;
  CastEventBuilder& SetName(const std::string& name) override;
  CastEventBuilder& SetTime(const base::TimeTicks& time) override;
  CastEventBuilder& SetTimezoneId(const std::string& timezone_id) override;
  CastEventBuilder& SetAppId(const std::string& app_id) override;
  CastEventBuilder& SetRemoteAppId(const std::string& remote_app_id) override;
  CastEventBuilder& SetSessionId(const std::string& session_id) override;
  CastEventBuilder& SetSdkVersion(const std::string& sdk_version) override;
  CastEventBuilder& SetMplVersion(const std::string& mpl_version) override;
  CastEventBuilder& SetConnectionInfo(
      const std::string& transport_connection_id,
      const std::string& virtual_connection_id) override;
  CastEventBuilder& SetGroupUuid(const std::string& group_uuid) override;
  CastEventBuilder& SetExtraValue(int64_t extra_value) override;
  CastEventBuilder& SetConversationKey(
      const std::string& conversation_key) override;
  CastEventBuilder& SetRequestId(int32_t request_id) override;
  CastEventBuilder& SetEventId(const std::string& event_id) override;
  CastEventBuilder& SetAoghRequestId(const std::string& request_id) override;
  CastEventBuilder& SetAoghLocalDeviceId(int64_t local_id) override;
  CastEventBuilder& SetAoghAgentId(const std::string& agent_id) override;
  CastEventBuilder& SetAoghStandardAgentId(
      const std::string& standard_agent_id) override;
  CastEventBuilder& SetUiVersion(const std::string& ui_version) override;
  CastEventBuilder& SetAuditReport(const std::string& audit_report) override;
  CastEventBuilder& SetDuoCoreVersion(int64_t version) override;
  CastEventBuilder& SetHotwordModelId(const std::string& model_id) override;
  CastEventBuilder& SetDiscoveryAppSubtype(const std::string& app_id) override;
  CastEventBuilder& SetDiscoveryNamespaceSubtype(
      const std::string& namespace_hash) override;
  CastEventBuilder& SetDiscoverySender(
      const net::IPAddressBytes& sender_ip) override;
  CastEventBuilder& SetDiscoveryUnicastFlag(bool uses_unicast) override;
  CastEventBuilder& SetFeatureVector(
      const std::vector<float>& features) override;
  CastEventBuilder& AddMetadata(const std::string& name,
                                int64_t value) override;
  CastEventBuilder& SetLaunchFrom(LaunchFrom launch_from) override;
  CastEventBuilder& MergeFrom(
      const ::metrics::CastLogsProto_CastEventProto* event_proto) override;

  ::metrics::CastLogsProto_CastEventProto* Build() override;

 private:
  std::unique_ptr<::metrics::CastLogsProto_CastEventProto> event_proto_;
  std::string unhashed_event_name_;
};

}  // namespace chromecast

#endif  // CHROMECAST_METRICS_CAST_EVENT_BUILDER_IMPL_H_
