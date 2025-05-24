// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_MEDIA_MEDIA_DRM_UTIL_H_
#define CHROMECAST_STARBOARD_MEDIA_MEDIA_DRM_UTIL_H_

#include <memory>
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
// This class is useful because StarboardDrmSampleInfo holds pointers that must
// live at least as long as the StarboardDrmSampleInfo itself. This is
// accomplished by managing all the relevant lifetimes in this class.
//
// Instances of this class should be created via DrmInfoWrapper::Create().
class DrmInfoWrapper {
 public:
  // Extracts and returns DRM info from `buffer`. If the buffer is not
  // encrypted, the StarboardDrmSampleInfo returned by GetDrmSampleInfo will be
  // null.
  static DrmInfoWrapper Create(const CastDecoderBuffer& buffer);

  // DrmInfoWrapper is movable but not copyable.
  DrmInfoWrapper(DrmInfoWrapper&& other);
  DrmInfoWrapper& operator=(DrmInfoWrapper&& other);
  DrmInfoWrapper(const DrmInfoWrapper&) = delete;
  DrmInfoWrapper& operator=(const DrmInfoWrapper&) = delete;

  ~DrmInfoWrapper();

  // Returns a pointer to the DrmSampleInfo owned by this wrapper, or null if
  // there is no sample info.
  StarboardDrmSampleInfo* GetDrmSampleInfo();

 private:
  // Constructs a DrmInfoWrapper for unencrypted content. GetDrmSampleInfo()
  // will return null.
  DrmInfoWrapper();

  // Constructs a DrmInfoWrapper representing encrypted content.
  DrmInfoWrapper(
      std::unique_ptr<StarboardDrmSampleInfo> drm_sample_info,
      std::unique_ptr<std::vector<StarboardDrmSubSampleMapping>> mappings);

  // unique_ptrs are used here for the sake of pointer stability, so that moving
  // a DrmInfoWrapper does not require updating any code that referenced the
  // pointers.
  std::unique_ptr<StarboardDrmSampleInfo> drm_sample_info_;
  std::unique_ptr<std::vector<StarboardDrmSubSampleMapping>>
      subsample_mappings_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_MEDIA_MEDIA_DRM_UTIL_H_
