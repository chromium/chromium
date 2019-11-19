// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/mixer_service/conversions.h"

#include "base/logging.h"

namespace chromecast {
namespace media {
namespace mixer_service {

media::SampleFormat ConvertSampleFormat(SampleFormat format) {
  switch (format) {
    case SAMPLE_FORMAT_INT16_I:
      return kSampleFormatS16;
    case SAMPLE_FORMAT_INT32_I:
      return kSampleFormatS32;
    case SAMPLE_FORMAT_FLOAT_I:
      return kSampleFormatF32;
    case SAMPLE_FORMAT_INT16_P:
      return kSampleFormatPlanarS16;
    case SAMPLE_FORMAT_INT32_P:
      return kSampleFormatPlanarS32;
    case SAMPLE_FORMAT_FLOAT_P:
      return kSampleFormatPlanarF32;
    default:
      NOTREACHED() << "Unknown sample format " << format;
  }
  return kSampleFormatS16;
}

SampleFormat ConvertSampleFormat(media::SampleFormat format) {
  switch (format) {
    case kSampleFormatS16:
      return SAMPLE_FORMAT_INT16_I;
    case kSampleFormatS32:
      return SAMPLE_FORMAT_INT32_I;
    case kSampleFormatF32:
      return SAMPLE_FORMAT_FLOAT_I;
    case kSampleFormatPlanarS16:
      return SAMPLE_FORMAT_INT16_P;
    case kSampleFormatPlanarS32:
      return SAMPLE_FORMAT_INT32_P;
    case kSampleFormatPlanarF32:
      return SAMPLE_FORMAT_FLOAT_P;
    default:
      NOTREACHED() << "Unhandled sample format " << format;
  }
  return SAMPLE_FORMAT_INT16_I;
}

int GetSampleSizeBytes(mixer_service::SampleFormat format) {
  if (format == SAMPLE_FORMAT_INT16_I || format == SAMPLE_FORMAT_INT16_P) {
    return 2;
  }
  return 4;
}

static_assert(static_cast<int>(CONTENT_TYPE_MEDIA) ==
                  static_cast<int>(AudioContentType::kMedia),
              "Content type enums don't match for media");
static_assert(static_cast<int>(CONTENT_TYPE_ALARM) ==
                  static_cast<int>(AudioContentType::kAlarm),
              "Content type enums don't match for alarm");
static_assert(static_cast<int>(CONTENT_TYPE_COMMUNICATION) ==
                  static_cast<int>(AudioContentType::kCommunication),
              "Content type enums don't match for communication");
static_assert(static_cast<int>(CONTENT_TYPE_OTHER) ==
                  static_cast<int>(AudioContentType::kOther),
              "Content type enums don't match for other");

ContentType ConvertContentType(media::AudioContentType content_type) {
  return static_cast<ContentType>(content_type);
}

media::AudioContentType ConvertContentType(ContentType type) {
  return static_cast<media::AudioContentType>(type);
}

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast
