// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/common/translate_util.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "components/translate/core/common/translate_switches.h"

namespace translate {

const char kSecurityOrigin[] = "https://translate.googleapis.com/";

// The feature is explicitly disabled on Webview and Weblayer.
// TODO(crbug.com/40819484): Enable the feature on Webview.
// TODO(crbug.com/40790180): Enable the feature on WebLayer.
BASE_FEATURE(kTFLiteLanguageDetectionEnabled,
             "TFLiteLanguageDetectionEnabled",
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

GURL GetTranslateSecurityOrigin() {
  std::string security_origin(kSecurityOrigin);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kTranslateSecurityOrigin)) {
    security_origin =
        command_line->GetSwitchValueASCII(switches::kTranslateSecurityOrigin);
  }
  return GURL(security_origin);
}

bool IsTFLiteLanguageDetectionEnabled() {
  return base::FeatureList::IsEnabled(kTFLiteLanguageDetectionEnabled);
}

float GetTFLiteLanguageDetectionThreshold() {
  return base::GetFieldTrialParamByFeatureAsDouble(
      kTFLiteLanguageDetectionEnabled, "reliability_threshold", .7);
}

BASE_FEATURE(kTranslateAutoSnackbars,
             "TranslateAutoSnackbars",
             base::FEATURE_ENABLED_BY_DEFAULT);

int GetAutoAlwaysThreshold() {
  static constexpr base::FeatureParam<int> auto_always_threshold{
      &kTranslateAutoSnackbars, "AutoAlwaysThreshold", 5};
  return auto_always_threshold.Get();
}

int GetAutoNeverThreshold() {
  static constexpr base::FeatureParam<int> auto_never_threshold{
      &kTranslateAutoSnackbars, "AutoNeverThreshold", 20};
  return auto_never_threshold.Get();
}

int GetMaximumNumberOfAutoAlways() {
  static constexpr base::FeatureParam<int> auto_always_maximum{
      &kTranslateAutoSnackbars, "AutoAlwaysMaximum", 2};
  return auto_always_maximum.Get();
}

int GetMaximumNumberOfAutoNever() {
  static constexpr base::FeatureParam<int> auto_never_maximum{
      &kTranslateAutoSnackbars, "AutoNeverMaximum", 2};
  return auto_never_maximum.Get();
}

}  // namespace translate
