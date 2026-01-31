// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/common/translate_util.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "components/translate/core/common/translate_switches.h"

namespace {
// The default number of times user should consecutively translate for "Always
// Translate" to automatically trigger.
constexpr int kAutoAlwaysThreshold = 5;
// The default number of times user should consecutively dismiss the translate
// infobar for "Never Translate" to automatically trigger.
constexpr int kAutoNeverThreshold = 20;
// The default maximum number of times "Always Translate" is automatically
// triggered.
constexpr int kMaxNumberOfAutoAlways = 2;
// The default maximum number of times "Never Translate" is automatically
// triggered.
constexpr int kMaxNumberOfAutoNever = 2;
}  // namespace

namespace translate {

const char kSecurityOrigin[] = "https://translate.googleapis.com/";

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
// The feature is explicitly disabled on WebView.
// TODO(crbug.com/40819484): Enable the feature on WebView.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return true;
#else
  return false;
#endif
}

int GetAutoAlwaysThreshold() {
  return kAutoAlwaysThreshold;
}

int GetAutoNeverThreshold() {
  return kAutoNeverThreshold;
}

int GetMaximumNumberOfAutoAlways() {
  return kMaxNumberOfAutoAlways;
}

int GetMaximumNumberOfAutoNever() {
  return kMaxNumberOfAutoNever;
}

}  // namespace translate
