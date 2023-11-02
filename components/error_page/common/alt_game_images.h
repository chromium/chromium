// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ERROR_PAGE_COMMON_ALT_GAME_IMAGES_H_
#define COMPONENTS_ERROR_PAGE_COMMON_ALT_GAME_IMAGES_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace error_page {

BASE_DECLARE_FEATURE(kNetErrorAltGameMode);
extern const base::FeatureParam<std::string> kNetErrorAltGameModeKey;

// Gets the value of kNetErrorAltGameMode.
bool EnableAltGameMode();

// Returns a data URL corresponding to the image ID and scale.
std::string GetAltGameImage(int image_id, int scale);

// Returns an image ID.
int ChooseAltGame();

}  // namespace error_page

#endif  // COMPONENTS_ERROR_PAGE_COMMON_ALT_GAME_IMAGES_H_
