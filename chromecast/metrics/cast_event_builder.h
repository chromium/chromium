// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_METRICS_CAST_EVENT_BUILDER_H_
#define CHROMECAST_METRICS_CAST_EVENT_BUILDER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

namespace base {
class TimeTicks;
}  // namespace base

namespace metrics {
class CastLogsProto_CastEventProto;
}

namespace net {
class IPAddressBytes;
}

namespace chromecast {

// Builder for CastLogsProto_CastEventProto.
class CastEventBuilder {
 public:
  // TODO(sanfin): as in the Build() method below, ideally this should just be
  // the proto-generated enum, but we don't want to depend on the metrics
  // component for now (in particular, because this requires generating the
  // proto headers before compiling any translation unit that #includes this
  // header, which is complicated by the fact that some code uses this header
  // that is compiled outside the gn build system.)
  enum LaunchFrom {
    FROM_UNKNOWN,
    FROM_LOCAL,
    FROM_DIAL,
    FROM_CAST_V2,
    FROM_CCS,
  };

  // This is used to preclude a header file dependency on the header generated
  // from third_party/metrics_proto/cast_logs.proto.
  static void SetLaunchFromProto(::metrics::CastLogsProto_CastEventProto* out,
                                 LaunchFrom launch_from);

  virtual ~CastEventBuilder() {}

  // Returns the unhashed event name set for this builder.
  virtual std::string GetName() = 0;

  virtual CastEventBuilder& SetName(const std::string& name) = 0;
  virtual CastEventBuilder& SetTime(const base::TimeTicks& time) = 0;

  virtual CastEventBuilder& SetTimezoneId(const std::string& timezone_id) = 0;
  virtual CastEventBuilder& SetAppId(const std::string& app_id) = 0;
  virtual CastEventBuilder& SetRemoteAppId(
      const std::string& remote_app_id) = 0;
  virtual CastEventBuilder& SetSessionId(const std::string& session_id) = 0;
  virtual CastEventBuilder& SetSdkVersion(const std::string& sdk_version) = 0;
  virtual CastEventBuilder& SetMplVersion(const std::string& mpl_version) = 0;
  virtual CastEventBuilder& SetConnectionInfo(
      const std::string& transport_connection_id,
      const std::string& virtual_connection_id) = 0;
  virtual CastEventBuilder& SetGroupUuid(const std::string& group_uuid) = 0;
  virtual CastEventBuilder& SetExtraValue(int64_t extra_value) = 0;
  virtual CastEventBuilder& SetConversationKey(
      const std::string& conversation_key) = 0;
  virtual CastEventBuilder& SetRequestId(int32_t request_id) = 0;
  virtual CastEventBuilder& SetEventId(const std::string& event_id) = 0;
  virtual CastEventBuilder& SetAoghRequestId(const std::string& request_id) = 0;
  virtual CastEventBuilder& SetAoghLocalDeviceId(int64_t local_id) = 0;
  virtual CastEventBuilder& SetAoghAgentId(const std::string& agent_id) = 0;
  virtual CastEventBuilder& SetAoghStandardAgentId(
      const std::string& standard_agent_id) = 0;
  virtual CastEventBuilder& SetUiVersion(const std::string& ui_version) = 0;
  virtual CastEventBuilder& SetAuditReport(const std::string& audit_report) = 0;
  virtual CastEventBuilder& SetDuoCoreVersion(int64_t version) = 0;
  virtual CastEventBuilder& SetHotwordModelId(const std::string& model_id) = 0;

  // Only used for reporting discovery related events. Sets the application
  // subtype related to the event, where |app_id| is the 8-byte hex string
  // for the given V2 app.
  virtual CastEventBuilder& SetDiscoveryAppSubtype(
      const std::string& app_id) = 0;
  // Only used for reporting discovery related events. Sets the namespace
  // subtype related to the event, where |namespace_hash| is the SHA-1 hash
  // string for the given V2 app namespace.
  virtual CastEventBuilder& SetDiscoveryNamespaceSubtype(
      const std::string& namespace_hash) = 0;
  // Only used for reporting discovery related events. Sets the sender IP
  // address associated with the given event. Used to track requests and
  // responses on a per sender basis. |sender_ip| is a vector containing each
  // byte in the IP address in network order.
  virtual CastEventBuilder& SetDiscoverySender(
      const net::IPAddressBytes& sender_ip) = 0;
  // Only used for reported discovery related events. Sets/unsets the unicast
  // flag used to specify what type of discovery is occurring, and to compare
  // unicast vs. multicast usage and reliability.
  //
  // TODO(maclellant): Refactor discovery metrics into more general events that
  // can be shared between all the discovery protocols. Right now this unicast
  // flag is really only specific to MDNS to signal the type of request or
  // response.
  virtual CastEventBuilder& SetDiscoveryUnicastFlag(bool uses_unicast) = 0;

  virtual CastEventBuilder& SetFeatureVector(
      const std::vector<float>& features) = 0;
  virtual CastEventBuilder& AddMetadata(const std::string& name,
                                        int64_t value) = 0;

  virtual CastEventBuilder& SetLaunchFrom(LaunchFrom launch_from) = 0;

  // Populates fields from the provided CastEventProto. Similar to the protobuf
  // MergeFrom method, singular fields are overwritten and repeated fields like
  // metadata are concatenated.
  virtual CastEventBuilder& MergeFrom(
      const ::metrics::CastLogsProto_CastEventProto* event_proto) = 0;

  // Build the proto, caller takes ownership.
  // TODO(gfhuang): Ideally this should be std::unique_ptr, but we don't want to
  // have cast to depend on metrics component for now.
  virtual ::metrics::CastLogsProto_CastEventProto* Build() = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_METRICS_CAST_EVENT_BUILDER_H_
