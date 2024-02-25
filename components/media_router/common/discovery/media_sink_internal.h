// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_DISCOVERY_MEDIA_SINK_INTERNAL_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_DISCOVERY_MEDIA_SINK_INTERNAL_H_

#include <utility>

#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/providers/cast/channel/cast_device_capability.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "url/gurl.h"

namespace media_router {

// Default Cast control port to open Cast Socket.
static constexpr int kCastControlPort = 8009;

// The method by which the cast sink was discovered.
enum class CastDiscoveryType {
  kMdns,
  kDial,
  kAccessCodeManualEntry,
  kAccessCodeRememberedDevice,
};

// Extra data for DIAL media sink.
struct DialSinkExtraData {
  net::IPAddress ip_address;

  // Model name of the sink.
  std::string model_name;

  // The base URL used for DIAL operations.
  GURL app_url;

  DialSinkExtraData();
  DialSinkExtraData(const DialSinkExtraData& other);
  DialSinkExtraData(DialSinkExtraData&& other);
  ~DialSinkExtraData();

  bool operator==(const DialSinkExtraData& other) const;
};

// Extra data for Cast media sink.
struct CastSinkExtraData {
  net::IPEndPoint ip_endpoint;

  int port = 0;

  // Model name of the sink.
  std::string model_name;

  // An enum set representing the capabilities of the sink. The enum values are
  // defined in cast_device_capability.h.
  cast_channel::CastDeviceCapabilitySet capabilities;

  // ID of Cast channel opened for the sink. The caller must set this value to a
  // valid cast_channel_id. The cast_channel_id may change over time as the
  // browser reconnects to a device.
  int cast_channel_id = 0;

  // The method used to discover the cast sink.
  CastDiscoveryType discovery_type = CastDiscoveryType::kMdns;

  CastSinkExtraData();
  CastSinkExtraData(const CastSinkExtraData& other);
  CastSinkExtraData(CastSinkExtraData&& other);
  ~CastSinkExtraData();

  bool operator==(const CastSinkExtraData& other) const;
};

// Represents a media sink discovered by MediaSinkService. It is used by
// MediaSinkService to push MediaSinks with extra data to the
// MediaRouteProvider, and it is not exposed to users of MediaRouter.
class MediaSinkInternal {
 public:
  // Used by mojo.
  MediaSinkInternal();

  // Used by MediaSinkService to create media sinks.
  MediaSinkInternal(const MediaSink& sink, const DialSinkExtraData& dial_data);
  MediaSinkInternal(const MediaSink& sink, const CastSinkExtraData& cast_data);

  // Used to push instance of this class into vector.
  MediaSinkInternal(const MediaSinkInternal& other);
  MediaSinkInternal(MediaSinkInternal&& other) noexcept;

  ~MediaSinkInternal();

  MediaSinkInternal& operator=(const MediaSinkInternal& other);
  MediaSinkInternal& operator=(MediaSinkInternal&& other) noexcept;
  bool operator==(const MediaSinkInternal& other) const;
  bool operator!=(const MediaSinkInternal& other) const;
  // Sorted by sink id.
  bool operator<(const MediaSinkInternal& other) const;

  void set_sink(const MediaSink& sink);
  const MediaSink& sink() const { return sink_; }
  MediaSink& sink() { return sink_; }

  // TOOD(jrw): Use this method where appropriate.
  const MediaSink::Id& id() const { return sink_.id(); }

  void set_dial_data(const DialSinkExtraData& dial_data);

  // Must only be called if the sink is a DIAL sink.
  const DialSinkExtraData& dial_data() const;

  void set_cast_data(const CastSinkExtraData& cast_data);

  // Must only be called if the sink is a Cast sink.
  const CastSinkExtraData& cast_data() const;
  CastSinkExtraData& cast_data();

  // TOOD(jrw): Use this method where appropriate.
  int cast_channel_id() const { return cast_data().cast_channel_id; }

  bool is_dial_sink() const { return sink_type_ == SinkType::DIAL; }
  bool is_cast_sink() const { return sink_type_ == SinkType::CAST; }

  static bool IsValidSinkId(const std::string& sink_id);

  // Returns processed device id without "uuid:" and "-", e.g. input
  // "uuid:6d238518-a574-eab1-017e-d0975c039081" and output
  // "6d238518a574eab1017ed0975c039081"
  static std::string ProcessDeviceUUID(const std::string& device_uuid);

 private:
  void InternalCopyConstructFrom(const MediaSinkInternal& other);
  void InternalMoveConstructFrom(MediaSinkInternal&& other);
  void InternalCleanup();

  enum class SinkType { GENERIC, DIAL, CAST };

  MediaSink sink_;

  SinkType sink_type_;

  union {
    // Set if sink is DIAL sink.
    DialSinkExtraData dial_data_;

    // Set if sink is Cast sink.
    CastSinkExtraData cast_data_;
  };
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_DISCOVERY_MEDIA_SINK_INTERNAL_H_
