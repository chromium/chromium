// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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
#include "components/color/color_mixers.h"
#include "ui/color/color_mixers.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_utils.h"

#define STRINGIZE_COLOR_IDS
#include "ui/color/color_id_macros.inc"

// clang-format off
const char* enum_names[] = {
  COLOR_IDS
  COMPONENTS_COLOR_IDS
  CHROME_COLOR_IDS
};
// clang-format on

// Note that this second include is not redundant. The second inclusion of the
// .inc file serves to undefine the macros the first inclusion defined.
#include "ui/color/color_id_macros.inc"

// Longest color name, plus a space.  Currently, "SK_ColorTRANSPARENT ".
constexpr size_t kColorColumnWidth = 19 + 1;

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
  const auto add_mixers = [](ui::ColorProvider* provider, auto color_mode,
                             auto contrast_mode) {
    ui::ColorProviderKey key;
    key.color_mode = color_mode;
    key.contrast_mode = contrast_mode;
    ui::AddColorMixers(provider, key);
    color::AddComponentsColorMixers(provider, key);
    AddChromeColorMixers(provider, key);
  };
  ui::ColorProvider light_provider, dark_provider, light_high_contrast_provider,
      dark_high_contrast_provider;
  add_mixers(&light_provider, ui::ColorProviderKey::ColorMode::kLight,
             ui::ColorProviderKey::ContrastMode::kNormal);
  add_mixers(&dark_provider, ui::ColorProviderKey::ColorMode::kDark,
             ui::ColorProviderKey::ContrastMode::kNormal);
  add_mixers(&light_high_contrast_provider,
             ui::ColorProviderKey::ColorMode::kLight,
             ui::ColorProviderKey::ContrastMode::kHigh);
  add_mixers(&dark_high_contrast_provider,
             ui::ColorProviderKey::ColorMode::kDark,
             ui::ColorProviderKey::ContrastMode::kHigh);

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
