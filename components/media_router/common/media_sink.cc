// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/media_sink.h"

#include "base/i18n/string_compare.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/icu/source/i18n/unicode/coll.h"

namespace media_router {

MediaSink::MediaSink(const MediaSink::Id& sink_id,
                     const std::string& name,
                     SinkIconType icon_type,
                     mojom::MediaRouteProviderId provider_id)
    : sink_id_(sink_id),
      name_(name),
      icon_type_(icon_type),
      provider_id_(provider_id) {}

MediaSink::MediaSink(const MediaSink& other) = default;
MediaSink::MediaSink(MediaSink&& other) noexcept = default;
MediaSink::MediaSink() = default;
MediaSink::~MediaSink() = default;

MediaSink& MediaSink::operator=(const MediaSink& other) = default;
MediaSink& MediaSink::operator=(MediaSink&& other) noexcept = default;

bool MediaSink::operator==(const MediaSink& other) const {
  return sink_id_ == other.sink_id_ && name_ == other.name_ &&
         icon_type_ == other.icon_type_ && provider_id_ == other.provider_id_;
}

bool MediaSink::operator!=(const MediaSink& other) const {
  return !operator==(other);
}

bool MediaSink::CompareUsingCollator(const MediaSink& other,
                                     const icu::Collator* collator) const {
  if (icon_type_ != other.icon_type_)
    return icon_type_ < other.icon_type_;

  if (collator) {
    std::u16string this_name = base::UTF8ToUTF16(name_);
    std::u16string other_name = base::UTF8ToUTF16(other.name_);
    UCollationResult result = base::i18n::CompareString16WithCollator(
        *collator, this_name, other_name);
    if (result != UCOL_EQUAL)
      return result == UCOL_LESS;
  } else {
    // Fall back to simple string comparison if collator is not
    // available.
    int val = name_.compare(other.name_);
    if (val)
      return val < 0;
  }

  return sink_id_ < other.sink_id_;
}

}  // namespace media_router
