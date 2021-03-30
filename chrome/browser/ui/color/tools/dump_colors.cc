// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This command-line program dumps the computed values of all color IDs to
// stdout.

#include <iomanip>
#include <ios>
#include <iostream>
#include <string>

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_mixers.h"
#include "chrome/browser/ui/color/omnibox_color_mixers.h"
#include "ui/color/color_mixers.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_utils.h"

#define STRINGIZE_COLOR_IDS
#include "ui/color/color_id_macros.inc"

// clang-format off
const char* enum_names[] = {
  COLOR_IDS
  CHROME_COLOR_IDS
};
// clang-format on

#include "ui/color/color_id_macros.inc"

#if defined(OS_MAC)
#include "ui/color/color_mixers.h"
#endif

constexpr size_t kColorColumnWidth = 19 + 1;  // '#xxxxxxxx '/'#xxxxxxxx\n'

std::string SkColorToString(SkColor color) {
  std::string color_string = ui::SkColorName(color);
  // Don't use the rgba() representation here. Just fall back to simple hex.
  if (color_string.find_first_of("rgb") == 0)
    color_string = base::StringPrintf("#%.8x", color);
  // Now format it into the necessary space.
  return base::StringPrintf("%-*s", int{kColorColumnWidth},
                            color_string.c_str());
}

int main(int argc, const char* argv[]) {
  const auto add_mixers = [](ui::ColorProvider* provider, bool dark_window,
                             bool high_contrast) {
    // TODO(pkasting): Use standard provider setup functions once those exist.
    ui::AddCoreDefaultColorMixer(provider, dark_window, high_contrast);
    ui::AddNativeCoreColorMixer(provider, dark_window, high_contrast);
    ui::AddUiColorMixer(provider, dark_window, high_contrast);
    ui::AddNativeUiColorMixer(provider, dark_window, high_contrast);
    ui::AddNativePostprocessingMixer(provider);
    AddChromeColorMixers(provider);
    AddOmniboxColorMixers(provider, false);
  };
  ui::ColorProvider light_provider, dark_provider, light_high_contrast_provider,
      dark_high_contrast_provider;
  add_mixers(&light_provider, false, false);
  add_mixers(&dark_provider, true, false);
  add_mixers(&light_high_contrast_provider, false, true);
  add_mixers(&dark_high_contrast_provider, true, true);

  size_t longest_name = 0;
  for (const char* name : enum_names)
    longest_name = std::max(longest_name, strlen(name));
  ++longest_name;  // For trailing space.

  std::cout << std::setfill(' ') << std::left;
  std::cout << std::setw(longest_name) << "ID";
  std::cout << std::setw(kColorColumnWidth) << "Light";
  std::cout << std::setw(kColorColumnWidth) << "Dark";
  std::cout << std::setw(kColorColumnWidth) << "Light High Contrast";
  std::cout << "Dark High Contrast\n";
  std::cout << std::setfill('-') << std::right;
  std::cout << std::setw(longest_name) << ' ';
  std::cout << std::setw(kColorColumnWidth) << ' ';
  std::cout << std::setw(kColorColumnWidth) << ' ';
  std::cout << std::setw(kColorColumnWidth) << ' ';
  std::cout << std::setw(kColorColumnWidth) << '\n';

  for (ui::ColorId id = ui::kUiColorsStart; id < kChromeColorsEnd; ++id) {
    std::cout << std::setfill(' ') << std::left;
    std::cout << std::setw(longest_name) << enum_names[id];
    std::cout << SkColorToString(light_provider.GetColor(id));
    std::cout << SkColorToString(dark_provider.GetColor(id));
    std::cout << SkColorToString(light_high_contrast_provider.GetColor(id));
    std::cout << SkColorToString(dark_high_contrast_provider.GetColor(id))
              << '\n';
  }

  std::cout.flush();

  return 0;
}
