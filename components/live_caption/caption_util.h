// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_CAPTION_UTIL_H_
#define COMPONENTS_LIVE_CAPTION_CAPTION_UTIL_H_

#include <optional>

#include "components/prefs/pref_service.h"
#include "ui/native_theme/caption_style.h"

class PrefService;

namespace captions {

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_MAC)
extern const char kCaptionSettingsUrl[];
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_MAC)

std::optional<ui::CaptionStyle> GetCaptionStyleFromUserSettings(
    PrefService* prefs,
    bool record_metrics);

// Returns whether the Live Caption feature is supported in Chrome. This can
// depend on e.g. Chrome feature flags, platform/OS, supported CPU instructions.
bool IsLiveCaptionFeatureSupported();

std::string GetCaptionSettingsUrl();

}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_CAPTION_UTIL_H_
