// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CDM_CHROMECAST_INIT_DATA_H_
#define CHROMECAST_MEDIA_CDM_CHROMECAST_INIT_DATA_H_

#include <stdint.h>

#include <vector>

namespace chromecast {
namespace media {

enum class InitDataMessageType {
  UNKNOWN = 0x0,
  CUSTOM_DATA = 0x1,
  ENABLE_SECURE_STOP = 0x2,
  END
};

// Structured data for EME initialization as parsed from an initData blob.
struct ChromecastInitData {
  ChromecastInitData();
  ~ChromecastInitData();

  InitDataMessageType type;
  std::vector<uint8_t> data;
};

// Searches for a ChromecastInitData blob inside a CENC |init_data| message of
// type |type|. If such a blob is found, returns true and fills
// |chromecast_init_data_out|. If not found, returns false.
bool FindChromecastInitData(const std::vector<uint8_t>& init_data,
                            InitDataMessageType type,
                            ChromecastInitData* chromecast_init_data_out);

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CDM_CHROMECAST_INIT_DATA_H_
