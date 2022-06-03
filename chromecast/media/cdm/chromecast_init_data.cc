// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cdm/chromecast_init_data.h"

#include "base/check.h"
#include "media/base/bit_reader.h"
#include "media/cdm/cenc_utils.h"

namespace chromecast {
namespace media {

#define RCHECK(x)   \
  do {              \
    if (!(x))       \
      return false; \
  } while (0)

namespace {

const uint8_t kChromecastPlayreadyUuid[] = {
    0x2b, 0xf8, 0x66, 0x80, 0xc6, 0xe5, 0x4e, 0x24,
    0xbe, 0x23, 0x0f, 0x81, 0x5a, 0x60, 0x6e, 0xb2};

}  // namespace

ChromecastInitData::ChromecastInitData() {
}

ChromecastInitData::~ChromecastInitData() {
}

bool FindChromecastInitData(const std::vector<uint8_t>& init_data,
                            InitDataMessageType type,
                            ChromecastInitData* chromecast_init_data_out) {
  // Chromecast initData assumes a CENC data format and searches for PSSH boxes
  // with SystemID |kChromecastPlayreadyUuid|. The PSSH box content is as
  // follows:
  // * |type| (2 bytes, InitDataMessageType)
  // * |data| (all remaining bytes)
  // Data may or may not be present and is specific to the given |type|.

  std::vector<uint8_t> pssh_data;
  if (!::media::GetPsshData(
          init_data, std::vector<uint8_t>(kChromecastPlayreadyUuid,
                                          kChromecastPlayreadyUuid +
                                              sizeof(kChromecastPlayreadyUuid)),
          &pssh_data)) {
    return false;
  }

  ::media::BitReader reader(pssh_data.data(), pssh_data.size());

  uint16_t msg_type;
  RCHECK(reader.ReadBits(2 * 8, &msg_type));
  RCHECK(msg_type < static_cast<uint16_t>(InitDataMessageType::END));
  RCHECK(msg_type == static_cast<uint16_t>(type));

  chromecast_init_data_out->type = static_cast<InitDataMessageType>(msg_type);
  chromecast_init_data_out->data.assign(
      pssh_data.begin() + reader.bits_read() / 8, pssh_data.end());
  return true;
}

}  // namespace media
}  // namespace chromecast
