// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TEST_PIXEL_TEST_CONFIGURATION_MIXIN_H_
#define CHROME_BROWSER_UI_TEST_PIXEL_TEST_CONFIGURATION_MIXIN_H_

#include <memory>

#include "base/scoped_environment_variable_override.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"

// Mixin to automatically set up generic pixel test configurations.
class PixelTestConfigurationMixin : public InProcessBrowserTestMixin {
 public:
  explicit PixelTestConfigurationMixin(InProcessBrowserTestMixinHost* host,
                                       bool use_dark_theme,
                                       bool use_right_to_left_language);
  PixelTestConfigurationMixin(const PixelTestConfigurationMixin&) = delete;
  PixelTestConfigurationMixin& operator=(const PixelTestConfigurationMixin&) =
      delete;

  // InProcessBrowserTestMixin
  ~PixelTestConfigurationMixin() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

 private:
  const bool use_dark_theme_;
  const bool use_right_to_left_language_;
  std::unique_ptr<base::ScopedEnvironmentVariableOverride> scoped_env_override_;
};

#endif  // CHROME_BROWSER_UI_TEST_PIXEL_TEST_CONFIGURATION_MIXIN_H_
