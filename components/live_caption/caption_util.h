// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_CAPTION_UTIL_H_
#define COMPONENTS_LIVE_CAPTION_CAPTION_UTIL_H_

#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/native_theme/caption_style.h"

class PrefService;

namespace captions {

absl::optional<ui::CaptionStyle> GetCaptionStyleFromUserSettings(
    PrefService* prefs,
    bool record_metrics);

}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_CAPTION_UTIL_H_
