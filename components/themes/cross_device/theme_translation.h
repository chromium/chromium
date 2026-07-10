// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_THEMES_CROSS_DEVICE_THEME_TRANSLATION_H_
#define COMPONENTS_THEMES_CROSS_DEVICE_THEME_TRANSLATION_H_

#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/sync/protocol/theme_android_specifics.pb.h"
#include "components/sync/protocol/theme_ios_specifics.pb.h"
#include "components/sync/protocol/theme_specifics.pb.h"
#include "components/themes/cross_device/cross_device_theme_tracker.h"

namespace themes {

#if BUILDFLAG(IS_ANDROID)
DeviceThemeInfo<sync_pb::ThemeAndroidSpecifics> TranslateDesktop(
    const sync_pb::ThemeSpecifics& desktop_specifics);

DeviceThemeInfo<sync_pb::ThemeAndroidSpecifics> TranslateIos(
    const sync_pb::ThemeIosSpecifics& ios_specifics);
#else
DeviceThemeInfo<sync_pb::ThemeSpecifics> TranslateAndroid(
    const sync_pb::ThemeAndroidSpecifics& android_specifics);

DeviceThemeInfo<sync_pb::ThemeSpecifics> TranslateIos(
    const sync_pb::ThemeIosSpecifics& ios_specifics);
#endif

}  // namespace themes

#endif  // COMPONENTS_THEMES_CROSS_DEVICE_THEME_TRANSLATION_H_
