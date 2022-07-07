// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_UI_PHOTO_PICKER_ANDROID_FEATURES_H_
#define COMPONENTS_BROWSER_UI_PHOTO_PICKER_ANDROID_FEATURES_H_

#include "base/feature_list.h"

namespace photo_picker {
namespace features {

// Whether to use the Android stock media picker instead of the Chrome picker.
extern const base::Feature kAndroidMediaPickerSupport;

// Whether the media picker supports videos.
extern const base::Feature kPhotoPickerVideoSupport;

}  // namespace features
}  // namespace photo_picker

#endif  // COMPONENTS_BROWSER_UI_PHOTO_PICKER_ANDROID_FEATURES_H_
