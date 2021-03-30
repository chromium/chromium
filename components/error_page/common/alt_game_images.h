// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ERROR_PAGE_COMMON_ALT_GAME_IMAGES_H_
#define COMPONENTS_ERROR_PAGE_COMMON_ALT_GAME_IMAGES_H_

#include <string>

#include "base/feature_list.h"

namespace error_page {

extern const base::Feature kNetErrorAltGameMode;
extern const base::FeatureParam<std::string> kNetErrorAltGameModeKey;

// Gets the value of kNetErrorAltGameMode.
bool EnableAltGameMode();

// Image loading result.
struct AltGameImages {
  std::string common_1x;
  std::string specific_1x;
  std::string common_2x;
  std::string specific_2x;

  AltGameImages();
  ~AltGameImages();
};

// Load images into |result| and game type into |choice|. Returns true on
// success.
bool GetAltGameImages(AltGameImages* result, int* choice);

}  // namespace error_page

#endif  // COMPONENTS_ERROR_PAGE_COMMON_ALT_GAME_IMAGES_H_
