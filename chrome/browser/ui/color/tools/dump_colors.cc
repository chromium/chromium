// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This command-line program dumps the computed values of all color IDs to
// stdout.

#include <iomanip>
#include <ios>
#include <iostream>

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/color/color_mixers.h"
#include "ui/color/color_provider.h"

int main(int argc, const char* argv[]) {
  const auto add_mixers = [](ui::ColorProvider* provider, bool dark_window) {
    // TODO(pkasting): Use standard provider setup functions once those exist.
    ui::AddCoreDefaultColorMixers(provider, dark_window);
    ui::AddNativeColorMixers(provider);
    ui::AddUiColorMixers(provider);
  };
  ui::ColorProvider light_provider, dark_provider;
  add_mixers(&light_provider, false);
  add_mixers(&dark_provider, true);

  std::cout << std::setfill('0');
  for (ui::ColorId id = ui::kUiColorsStart; id < kChromeColorsEnd; ++id) {
    // TODO(pkasting): String names for IDs.
    std::cout << "ID: " << std::dec << std::setw(4) << id << std::hex
              << " Light: " << std::setw(8) << light_provider.GetColor(id)
              << " Dark: " << std::setw(8) << dark_provider.GetColor(id)
              << '\n';
  }
  std::cout.flush();

  return 0;
}
