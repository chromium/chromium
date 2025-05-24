// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cdm/common/widevine_drm_delegate_android.h"

#include <vector>

#include "media/cdm/cenc_utils.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

namespace cdm {

WidevineDrmDelegateAndroid::WidevineDrmDelegateAndroid() = default;

WidevineDrmDelegateAndroid::~WidevineDrmDelegateAndroid() = default;

const std::vector<uint8_t> WidevineDrmDelegateAndroid::GetUUID() const {
  return std::vector<uint8_t>(std::begin(kWidevineUuid),
                              std::end(kWidevineUuid));
}

bool WidevineDrmDelegateAndroid::OnCreateSession(
    const media::EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    std::vector<uint8_t>* init_data_out,
    std::vector<std::string>* /* optional_parameters_out */) {
  if (init_data_type != media::EmeInitDataType::CENC)
    return true;

  // Widevine MediaDrm plugin only accepts the "data" part of the PSSH box as
  // the init data when using MP4 container.
  return media::GetPsshData(init_data, GetUUID(), init_data_out);
}

}  // namespace cdm
