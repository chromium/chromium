// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_MEDIA_SINK_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_MEDIA_SINK_H_

#include <string>

#include "components/media_router/common/media_route_provider_helper.h"
#include "components/media_router/common/mojom/media_route_provider_id.mojom.h"
#include "third_party/icu/source/common/unicode/uversion.h"

namespace U_ICU_NAMESPACE {
class Collator;
}  // namespace U_ICU_NAMESPACE

namespace media_router {

// IconTypes are listed in the order in which sinks should be sorted.
// The values must match media_router::mojom::SinkIconType and
// ash::SinkIconType.
//
// NOTE: This enum is used for recording the MediaRouter.Sink.SelectedType
// metrics, so if we want to reorder it, we must create a separate enum that
// preserves the ordering, and map from this enum to the new one in
// MediaRouterMetrics::RecordMediaSinkType().
enum class SinkIconType {
  CAST = 0,
  CAST_AUDIO_GROUP = 1,
  CAST_AUDIO = 2,
  WIRED_DISPLAY = 6,
  GENERIC = 7,
  TOTAL_COUNT = 8  // Add new types above this line.
};

// Represents a sink to which media can be routed.
// TODO(zhaobin): convert MediaSink into a struct.
class MediaSink {
 public:
  using Id = std::string;

  MediaSink(const MediaSink::Id& sink_id,
            const std::string& name,
            SinkIconType icon_type,
            mojom::MediaRouteProviderId provider_id);
  MediaSink(const MediaSink& other);
  MediaSink(MediaSink&& other) noexcept;
  MediaSink();
  ~MediaSink();

  MediaSink& operator=(const MediaSink& other);
  MediaSink& operator=(MediaSink&& other) noexcept;

  void set_sink_id(const MediaSink::Id& sink_id) { sink_id_ = sink_id; }
  const MediaSink::Id& id() const { return sink_id_; }

  void set_name(const std::string& name) { name_ = name; }
  const std::string& name() const { return name_; }

  void set_icon_type(SinkIconType icon_type) { icon_type_ = icon_type; }
  SinkIconType icon_type() const { return icon_type_; }

  void set_provider_id(mojom::MediaRouteProviderId provider_id) {
    provider_id_ = provider_id;
  }
  mojom::MediaRouteProviderId provider_id() const { return provider_id_; }

  bool operator==(const MediaSink& other) const;
  bool operator!=(const MediaSink& other) const;

  // Compares |this| to |other| first by their icon types, then their names
  // using |collator|, and finally their IDs.
  bool CompareUsingCollator(const MediaSink& other,
                            const icu::Collator* collator) const;

 private:
  // Unique identifier for the MediaSink.
  MediaSink::Id sink_id_;

  // Descriptive name of the MediaSink.
  std::string name_;

  // The type of icon that corresponds with the MediaSink.
  SinkIconType icon_type_ = SinkIconType::GENERIC;

  // The ID of the MediaRouteProvider that the MediaSink belongs to.
  mojom::MediaRouteProviderId provider_id_;
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_MEDIA_SINK_H_
