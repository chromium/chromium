// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/caption_util.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/ui_base_switches.h"
#include "ui/native_theme/native_theme.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_ANDROID)
#include "components/soda/soda_util.h"
#endif

namespace {

// Returns whether the style is default or not. If the user has changed any of
// the captions settings from the default value, that is an interesting metric
// to observe.
bool IsDefaultStyle(std::optional<ui::CaptionStyle> style) {
  return (style.has_value() && style->text_size.empty() &&
          style->font_family.empty() && style->text_color.empty() &&
          style->background_color.empty() && style->text_shadow.empty());
}

// Adds !important to all captions styles. They should always override any
// styles added by the video author or by a user stylesheet. This is because in
// Chrome, there is an option to turn off captions styles, so any time the
// captions are on, the styles should take priority.
std::string AddCSSImportant(std::string css_string) {
  return css_string.empty() ? "" : css_string + " !important";
}

// Constructs the CaptionStyle struct from the caption-related preferences.
std::optional<ui::CaptionStyle> GetCaptionStyleFromPrefs(PrefService* prefs) {
  if (!prefs) {
    return std::nullopt;
  }

  ui::CaptionStyle style;

  style.text_size =
      AddCSSImportant(prefs->GetString(prefs::kAccessibilityCaptionsTextSize));
  style.font_family =
      AddCSSImportant(prefs->GetString(prefs::kAccessibilityCaptionsTextFont));
  if (!prefs->GetString(prefs::kAccessibilityCaptionsTextColor).empty()) {
    std::string text_color = base::StringPrintf(
        "rgba(%s,%s)",
        prefs->GetString(prefs::kAccessibilityCaptionsTextColor).c_str(),
        base::NumberToString(
            prefs->GetInteger(prefs::kAccessibilityCaptionsTextOpacity) / 100.0)
            .c_str());
    style.text_color = AddCSSImportant(text_color);
  }

  if (!prefs->GetString(prefs::kAccessibilityCaptionsBackgroundColor).empty()) {
    std::string background_color = base::StringPrintf(
        "rgba(%s,%s)",
        prefs->GetString(prefs::kAccessibilityCaptionsBackgroundColor).c_str(),
        base::NumberToString(
            prefs->GetInteger(prefs::kAccessibilityCaptionsBackgroundOpacity) /
            100.0)
            .c_str());
    style.background_color = AddCSSImportant(background_color);
  }

  style.text_shadow = AddCSSImportant(
      prefs->GetString(prefs::kAccessibilityCaptionsTextShadow));

  return style;
}

}  // namespace

namespace captions {

std::optional<ui::CaptionStyle> GetCaptionStyleFromUserSettings(
    PrefService* prefs,
    bool record_metrics) {
  // Apply native CaptionStyle parameters.
  std::optional<ui::CaptionStyle> style;

  // Apply native CaptionStyle parameters.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kForceCaptionStyle)) {
    style = ui::CaptionStyle::FromSpec(
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kForceCaptionStyle));
  }

  // Apply system caption style.
  if (!style) {
    ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForWeb();
    style = native_theme->GetSystemCaptionStyle();
    if (record_metrics && style.has_value()) {
      base::UmaHistogramBoolean(
          "Accessibility.CaptionSettingsLoadedFromSystemSettings",
          !IsDefaultStyle(style));
    }
  }

  // Apply caption style from preferences if system caption style is undefined.
  if (!style) {
    style = GetCaptionStyleFromPrefs(prefs);
    if (record_metrics && style.has_value()) {
      base::UmaHistogramBoolean("Accessibility.CaptionSettingsLoadedFromPrefs",
                                !IsDefaultStyle(style));
    }
  }

  return style;
}

bool IsLiveCaptionFeatureSupported() {
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_ANDROID)
  return speech::IsOnDeviceSpeechRecognitionSupported();
#else
  return false;
#endif  // !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_ANDROID)
}

std::string GetCaptionSettingsUrl() {
#if BUILDFLAG(IS_CHROMEOS)
  return "chrome://os-settings/audioAndCaptions";
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX)
  return "chrome://settings/captions";
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN)
  return base::win::GetVersion() >= base::win::Version::WIN10
             ? "chrome://settings/accessibility"
             : "chrome://settings/captions";
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
  return "chrome://settings/accessibility";
#endif  // BUILDFLAG(IS_MAC)

  NOTREACHED_IN_MIGRATION();
  return std::string();
}

}  // namespace captions
