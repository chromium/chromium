// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/discovery/media_sink_internal.h"

#include <variant>

#include "base/check.h"
#include "base/logging.h"
#include "base/strings/string_util.h"

namespace media_router {

MediaSinkInternal::MediaSinkInternal() = default;

MediaSinkInternal::MediaSinkInternal(const MediaSink& sink,
                                     const DialSinkExtraData& dial_data)
    : sink_(sink), extra_data_(dial_data) {}

MediaSinkInternal::MediaSinkInternal(const MediaSink& sink,
                                     const CastSinkExtraData& cast_data)
    : sink_(sink), extra_data_(cast_data) {}

MediaSinkInternal::MediaSinkInternal(const MediaSinkInternal&) = default;

MediaSinkInternal::MediaSinkInternal(MediaSinkInternal&&) noexcept = default;

MediaSinkInternal::~MediaSinkInternal() = default;

MediaSinkInternal& MediaSinkInternal::operator=(const MediaSinkInternal&) =
    default;

MediaSinkInternal& MediaSinkInternal::operator=(MediaSinkInternal&&) noexcept =
    default;

bool MediaSinkInternal::operator==(const MediaSinkInternal&) const = default;

bool MediaSinkInternal::operator<(const MediaSinkInternal& other) const {
  return sink_.id() < other.sink().id();
}

void MediaSinkInternal::set_sink(const MediaSink& sink) {
  sink_ = sink;
}

void MediaSinkInternal::set_dial_data(const DialSinkExtraData& dial_data) {
  DCHECK(!is_cast_sink());
  extra_data_ = dial_data;
}

const DialSinkExtraData& MediaSinkInternal::dial_data() const {
  return std::get<DialSinkExtraData>(extra_data_);
}

void MediaSinkInternal::set_cast_data(const CastSinkExtraData& cast_data) {
  DCHECK(!is_dial_sink());
  extra_data_ = cast_data;
}

const CastSinkExtraData& MediaSinkInternal::cast_data() const {
  return std::get<CastSinkExtraData>(extra_data_);
}

CastSinkExtraData& MediaSinkInternal::cast_data() {
  return std::get<CastSinkExtraData>(extra_data_);
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
  if (device_uuid.empty()) {
    return std::string();
  }

  std::string result = device_uuid;
  if (base::StartsWith(device_uuid, "uuid:", base::CompareCase::SENSITIVE)) {
    result = device_uuid.substr(5);
  }

  base::RemoveChars(result, "-", &result);
  return base::ToLowerASCII(result);
}

DialSinkExtraData::DialSinkExtraData() = default;
DialSinkExtraData::DialSinkExtraData(const DialSinkExtraData&) = default;
DialSinkExtraData::DialSinkExtraData(DialSinkExtraData&&) = default;
DialSinkExtraData::~DialSinkExtraData() = default;
DialSinkExtraData& DialSinkExtraData::operator=(const DialSinkExtraData&) =
    default;
DialSinkExtraData& DialSinkExtraData::operator=(DialSinkExtraData&&) = default;

bool DialSinkExtraData::operator==(const DialSinkExtraData& other) const {
  return ip_address == other.ip_address && model_name == other.model_name &&
         app_url == other.app_url;
}

CastSinkExtraData::CastSinkExtraData() = default;
CastSinkExtraData::CastSinkExtraData(const CastSinkExtraData&) = default;
CastSinkExtraData::CastSinkExtraData(CastSinkExtraData&&) = default;
CastSinkExtraData::~CastSinkExtraData() = default;
CastSinkExtraData& CastSinkExtraData::operator=(const CastSinkExtraData&) =
    default;
CastSinkExtraData& CastSinkExtraData::operator=(CastSinkExtraData&&) = default;

bool CastSinkExtraData::operator==(const CastSinkExtraData& other) const {
  return ip_endpoint == other.ip_endpoint && model_name == other.model_name &&
         capabilities == other.capabilities &&
         cast_channel_id == other.cast_channel_id &&
         discovery_type == other.discovery_type;
}

}  // namespace media_router
