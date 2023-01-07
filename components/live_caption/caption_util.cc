// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/caption_util.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/cpu.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_service.h"
#include "media/base/media_switches.h"
#include "ui/base/ui_base_switches.h"
#include "ui/native_theme/native_theme.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace {

// Returns whether the style is default or not. If the user has changed any of
// the captions settings from the default value, that is an interesting metric
// to observe.
bool IsDefaultStyle(absl::optional<ui::CaptionStyle> style) {
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
absl::optional<ui::CaptionStyle> GetCaptionStyleFromPrefs(PrefService* prefs) {
  if (!prefs) {
    return absl::nullopt;
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

absl::optional<ui::CaptionStyle> GetCaptionStyleFromUserSettings(
    PrefService* prefs,
    bool record_metrics) {
  // Apply native CaptionStyle parameters.
  absl::optional<ui::CaptionStyle> style;

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
  if (!base::FeatureList::IsEnabled(media::kLiveCaption))
    return false;

// Some Chrome OS devices do not support on-device speech.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!base::FeatureList::IsEnabled(ash::features::kOnDeviceSpeechRecognition))
    return false;
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!chromeos::BrowserParamsProxy::Get()->IsOndeviceSpeechSupported())
    return false;
#endif

#if BUILDFLAG(IS_LINUX)
  // Check if the CPU has the required instruction set to run the Speech
  // On-Device API (SODA) library.
  static bool has_sse41 = base::CPU().has_sse41();
  if (!has_sse41)
    return false;
#endif

#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
  // The Speech On-Device API (SODA) component does not support Windows on
  // arm64.
  return false;
#else
  return true;
#endif
}

}  // namespace captions
