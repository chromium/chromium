// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/discovery/media_sink_internal.h"

#include <new>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"

namespace media_router {

MediaSinkInternal::MediaSinkInternal(const MediaSink& sink,
                                     const DialSinkExtraData& dial_data)
    : sink_(sink), sink_type_(SinkType::DIAL), dial_data_(dial_data) {}

MediaSinkInternal::MediaSinkInternal(const MediaSink& sink,
                                     const CastSinkExtraData& cast_data)
    : sink_(sink), sink_type_(SinkType::CAST), cast_data_(cast_data) {}

MediaSinkInternal::MediaSinkInternal() : sink_type_(SinkType::GENERIC) {}

MediaSinkInternal::MediaSinkInternal(const MediaSinkInternal& other) {
  InternalCopyConstructFrom(other);
}

MediaSinkInternal::MediaSinkInternal(MediaSinkInternal&& other) noexcept {
  InternalMoveConstructFrom(std::move(other));
}

MediaSinkInternal::~MediaSinkInternal() {
  InternalCleanup();
}

MediaSinkInternal& MediaSinkInternal::operator=(
    const MediaSinkInternal& other) {
  if (this != &other) {
    InternalCleanup();
    InternalCopyConstructFrom(other);
  }
  return *this;
}

MediaSinkInternal& MediaSinkInternal::operator=(
    MediaSinkInternal&& other) noexcept {
  if (this != &other) {
    InternalCleanup();
    InternalMoveConstructFrom(std::move(other));
  }
  return *this;
}

bool MediaSinkInternal::operator==(const MediaSinkInternal& other) const {
  if (sink_type_ != other.sink_type_)
    return false;

  if (sink_ != other.sink_)
    return false;

  switch (sink_type_) {
    case SinkType::DIAL:
      return dial_data_ == other.dial_data_;
    case SinkType::CAST:
      return cast_data_ == other.cast_data_;
    case SinkType::GENERIC:
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool MediaSinkInternal::operator!=(const MediaSinkInternal& other) const {
  return !operator==(other);
}

bool MediaSinkInternal::operator<(const MediaSinkInternal& other) const {
  return sink_.id() < other.sink().id();
}

void MediaSinkInternal::set_sink(const MediaSink& sink) {
  sink_ = sink;
}

void MediaSinkInternal::set_dial_data(const DialSinkExtraData& dial_data) {
  DCHECK(sink_type_ != SinkType::CAST);
  InternalCleanup();

  sink_type_ = SinkType::DIAL;
  new (&dial_data_) DialSinkExtraData(dial_data);
}

const DialSinkExtraData& MediaSinkInternal::dial_data() const {
  DCHECK(is_dial_sink());
  return dial_data_;
}

void MediaSinkInternal::set_cast_data(const CastSinkExtraData& cast_data) {
  DCHECK(sink_type_ != SinkType::DIAL);
  InternalCleanup();

  sink_type_ = SinkType::CAST;
  new (&cast_data_) CastSinkExtraData(cast_data);
}

const CastSinkExtraData& MediaSinkInternal::cast_data() const {
  DCHECK(is_cast_sink());
  return cast_data_;
}

CastSinkExtraData& MediaSinkInternal::cast_data() {
  DCHECK(is_cast_sink());
  return cast_data_;
}

// static
bool MediaSinkInternal::IsValidSinkId(const std::string& sink_id) {
  if (sink_id.empty() || !base::IsStringASCII(sink_id)) {
    DLOG(WARNING) << "Invalid [sink_id]: " << sink_id;
    return false;
  }

  return true;
}

// static
std::string MediaSinkInternal::ProcessDeviceUUID(
    const std::string& device_uuid) {
  if (device_uuid.empty())
    return std::string();

  std::string result = device_uuid;
  if (base::StartsWith(device_uuid, "uuid:", base::CompareCase::SENSITIVE))
    result = device_uuid.substr(5);

  base::RemoveChars(result, "-", &result);
  return base::ToLowerASCII(result);
}

void MediaSinkInternal::InternalCopyConstructFrom(
    const MediaSinkInternal& other) {
  sink_ = other.sink_;
  sink_type_ = other.sink_type_;

  switch (sink_type_) {
    case SinkType::DIAL:
      new (&dial_data_) DialSinkExtraData(other.dial_data_);
      return;
    case SinkType::CAST:
      new (&cast_data_) CastSinkExtraData(other.cast_data_);
      return;
    case SinkType::GENERIC:
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void MediaSinkInternal::InternalMoveConstructFrom(MediaSinkInternal&& other) {
  sink_ = std::move(other.sink_);
  sink_type_ = other.sink_type_;

  switch (sink_type_) {
    case SinkType::DIAL:
      new (&dial_data_) DialSinkExtraData(std::move(other.dial_data_));
      return;
    case SinkType::CAST:
      new (&cast_data_) CastSinkExtraData(std::move(other.cast_data_));
      return;
    case SinkType::GENERIC:
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void MediaSinkInternal::InternalCleanup() {
  switch (sink_type_) {
    case SinkType::DIAL:
      dial_data_.~DialSinkExtraData();
      return;
    case SinkType::CAST:
      cast_data_.~CastSinkExtraData();
      return;
    case SinkType::GENERIC:
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

DialSinkExtraData::DialSinkExtraData() = default;
DialSinkExtraData::DialSinkExtraData(const DialSinkExtraData& other) = default;
DialSinkExtraData::DialSinkExtraData(DialSinkExtraData&& other) = default;
DialSinkExtraData::~DialSinkExtraData() = default;

bool DialSinkExtraData::operator==(const DialSinkExtraData& other) const {
  return ip_address == other.ip_address && model_name == other.model_name &&
         app_url == other.app_url;
}

CastSinkExtraData::CastSinkExtraData() = default;
CastSinkExtraData::CastSinkExtraData(const CastSinkExtraData& other) = default;
CastSinkExtraData::CastSinkExtraData(CastSinkExtraData&& other) = default;
CastSinkExtraData::~CastSinkExtraData() = default;

bool CastSinkExtraData::operator==(const CastSinkExtraData& other) const {
  return ip_endpoint == other.ip_endpoint && model_name == other.model_name &&
         capabilities == other.capabilities &&
         cast_channel_id == other.cast_channel_id &&
         discovery_type == other.discovery_type;
}

}  // namespace media_router
