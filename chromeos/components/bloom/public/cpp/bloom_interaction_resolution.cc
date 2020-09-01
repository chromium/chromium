// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/bloom/public/cpp/bloom_interaction_resolution.h"

namespace chromeos {
namespace bloom {

#define CASE(name)                       \
  case BloomInteractionResolution::name: \
    return #name;

std::string ToString(BloomInteractionResolution resolution) {
  switch (resolution) {
    CASE(kNormal);
    CASE(kNoAccessToken);
    CASE(kNoScreenshot);
    CASE(kServerError);
  }
}

}  // namespace bloom
}  // namespace chromeos
