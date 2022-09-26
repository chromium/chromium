// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_UI_PHOTO_PICKER_ANDROID_FEATURES_H_
#define COMPONENTS_BROWSER_UI_PHOTO_PICKER_ANDROID_FEATURES_H_

#include "base/feature_list.h"

namespace photo_picker {
namespace features {

// Whether to use the Android stock media picker instead of the Chrome picker.
BASE_DECLARE_FEATURE(kAndroidMediaPickerSupport);

// Whether the media picker supports videos.
BASE_DECLARE_FEATURE(kPhotoPickerVideoSupport);

}  // namespace features
}  // namespace photo_picker

#endif  // COMPONENTS_BROWSER_UI_PHOTO_PICKER_ANDROID_FEATURES_H_
