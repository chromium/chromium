// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/ui/test/pixel_test_configuration_mixin.h"
#include "ui/base/ui_base_switches.h"

PixelTestConfigurationMixin::PixelTestConfigurationMixin(
    InProcessBrowserTestMixinHost* host,
    bool use_dark_theme,
    bool use_right_to_left_language)
    : InProcessBrowserTestMixin(host),
      use_dark_theme_(use_dark_theme),
      use_right_to_left_language_(use_right_to_left_language) {}

PixelTestConfigurationMixin::~PixelTestConfigurationMixin() = default;

void PixelTestConfigurationMixin::SetUpCommandLine(
    base::CommandLine* command_line) {
  if (use_dark_theme_) {
    command_line->AppendSwitch(switches::kForceDarkMode);
  }
  if (use_right_to_left_language_) {
    const std::string language = "ar-XB";
    command_line->AppendSwitchASCII(switches::kLang, language);

    // On Linux & Lacros the command line switch has no effect, we need to use
    // environment variables to change the language.
    scoped_env_override_ =
        std::make_unique<base::ScopedEnvironmentVariableOverride>("LANGUAGE",
                                                                  language);
  }
}
