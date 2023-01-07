// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_DECODER_ADVERTISEMENT_DECODER_H_
#define CHROME_SERVICES_SHARING_NEARBY_DECODER_ADVERTISEMENT_DECODER_H_

#include <stdint.h>
#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "chrome/services/sharing/public/cpp/advertisement.h"

namespace sharing {

// A utility class to convert from a byte span in the form of
// [VERSION|VISIBILITY][SALT][ACCOUNT_IDENTIFIER][LEN][DEVICE_NAME],
// to an Advertisement.
// A device name indicates the advertisement is visible to everyone;
// a missing device name indicates the advertisement is contacts-only.
class AdvertisementDecoder {
 public:
  AdvertisementDecoder() = delete;
  ~AdvertisementDecoder() = delete;

  static std::unique_ptr<sharing::Advertisement> FromEndpointInfo(
      base::span<const uint8_t> endpoint_info);
};

}  // namespace sharing

#endif  // CHROME_SERVICES_SHARING_NEARBY_DECODER_ADVERTISEMENT_DECODER_H_
