// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_colors.h"

#include <array>
#include <utility>

#include "chrome/browser/new_tab_page/chrome_colors/generated_colors_info.h"
#include "chrome/browser/new_tab_page/chrome_colors/selected_colors_info.h"

namespace {

// Returns chrome color with ID |kCustomizeChromeColorIds[I]|.
template <size_t I>
constexpr chrome_colors::ColorInfo GetChromeColor() {
  constexpr int id = kCustomizeChromeColorIds[I];
  // We assume that chrome colors are stored sequentially ordered by their ID
  // starting at ID 1.
  constexpr auto chrome_color = chrome_colors::kGeneratedColorsInfo[id - 1];
  static_assert(chrome_color.id == id);
  return chrome_color;
}

template <std::size_t... I>
constexpr auto MakeCustomizeChromeColors(std::index_sequence<I...>) {
  return std::array<chrome_colors::ColorInfo, sizeof...(I)>{
      GetChromeColor<I>()...};
}

}  // namespace

const decltype(kCustomizeChromeColors) kCustomizeChromeColors =
    MakeCustomizeChromeColors(
        std::make_index_sequence<std::size(kCustomizeChromeColorIds)>{});
