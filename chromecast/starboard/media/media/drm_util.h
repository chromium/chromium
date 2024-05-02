// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Contains a helper function to get DRM info from buffers. This logic can be
// used by both the StarboardAudioDecoder and the StarboardVideoDecoder.

#ifndef CHROMECAST_STARBOARD_MEDIA_MEDIA_DRM_UTIL_H_
#define CHROMECAST_STARBOARD_MEDIA_MEDIA_DRM_UTIL_H_

#include <optional>
#include <vector>

#include "chromecast/public/media/cast_decoder_buffer.h"
#include "chromecast/public/media/cast_decrypt_config.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"

namespace chromecast {
namespace media {

// A wrapper for fields related to DRM. If GetDrmSampleInfo() returns a
// non-null value and subsample mappings are present, the lifetime of those
// subsamples is tied to the lifetime of the wrapper.
//
// After moving or destroying a DrmInfoWrapper, any pointers previously returned
// by GetDrmSampleInfo should not be dereferenced, as they are no longer valid.
class DrmInfoWrapper {
 public:
  // Constructs a DrmInfoWrapper for unencrypted content. GetDrmSampleInfo()
  // will return null.
  DrmInfoWrapper();

  // Constructs a DrmInfoWrapper representing encrypted content.
  // GetDrmSampleInfo() will return a populated StarboardDrmSampleInfo.
  DrmInfoWrapper(StarboardDrmSampleInfo drm_sample_info,
                 std::vector<StarboardDrmSubSampleMapping> mappings);

  // DrmInfoWrapper is movable but not copyable.
  DrmInfoWrapper(DrmInfoWrapper&& other);
  DrmInfoWrapper& operator=(DrmInfoWrapper&& other);
  DrmInfoWrapper(const DrmInfoWrapper&) = delete;
  DrmInfoWrapper& operator=(const DrmInfoWrapper&) = delete;

  ~DrmInfoWrapper();

  // Returns a pointer to the DrmSampleInfo owned by this wrapper, or null if
  // there is no sample info. The returned pointer must not be dereferenced
  // after moving or destroying this object.
  StarboardDrmSampleInfo* GetDrmSampleInfo();

 private:
  // Updates drm_sample_info->subsample_count and
  // drm_sample_info->subsample_mapping to refer to `subsample_mappings`. These
  // fields need to be updated when moving a DrmInfoWrapper.
  void UpdateSubsampleInfo();

  std::optional<StarboardDrmSampleInfo> drm_sample_info_;
  std::vector<StarboardDrmSubSampleMapping> subsample_mappings_;
};

// Extracts and returns DRM info from `buffer`. If the buffer is not encrypted,
// the StarboardDrmSampleInfo returned by GetDrmSampleInfo will be null.
DrmInfoWrapper GetDrmInfo(const CastDecoderBuffer& buffer);

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_MEDIA_MEDIA_DRM_UTIL_H_
