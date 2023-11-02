// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cdm/playready_drm_delegate_android.h"

#include "base/logging.h"
#include "chromecast/media/cdm/chromecast_init_data.h"

namespace chromecast {
namespace media {

const uint8_t kPlayreadyUuid[16] = {
    0x9a, 0x04, 0xf0, 0x79, 0x98, 0x40, 0x42, 0x86,
    0xab, 0x92, 0xe6, 0x5b, 0xe0, 0x88, 0x5f, 0x95};

PlayreadyDrmDelegateAndroid::PlayreadyDrmDelegateAndroid() {
}

PlayreadyDrmDelegateAndroid::~PlayreadyDrmDelegateAndroid() {
}

const ::media::UUID PlayreadyDrmDelegateAndroid::GetUUID() const {
  return ::media::UUID(kPlayreadyUuid,
                       kPlayreadyUuid + std::size(kPlayreadyUuid));
}

bool PlayreadyDrmDelegateAndroid::OnCreateSession(
    const ::media::EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    std::vector<uint8_t>* /* init_data_out */,
    std::vector<std::string>* optional_parameters_out) {
  if (init_data_type == ::media::EmeInitDataType::CENC) {
    ChromecastInitData custom_data;
    if (FindChromecastInitData(init_data, InitDataMessageType::CUSTOM_DATA,
                               &custom_data)) {
      optional_parameters_out->clear();
      optional_parameters_out->push_back("PRCustomData");
      optional_parameters_out->push_back(
          std::string(custom_data.data.begin(), custom_data.data.end()));
      LOG(INFO) << "Including " << custom_data.data.size()
                << " bytes of custom PlayReady data";
    }
  }
  return true;
}

}  // namespace media
}  // namespace chromecast
