// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_style_views.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/common/horizontal_tab_style_views.h"
#include "chrome/browser/ui/views/tabs/common/vertical_tab_style_views.h"
#include "ui/gfx/color_utils.h"

// static
std::u16string ui::metadata::TypeConverter<TabStyle::TabColors>::ToString(
    ui::metadata::ArgType<TabStyle::TabColors> source_value) {
  return base::ASCIIToUTF16(base::StrCat(
      {"{", color_utils::SkColorToRgbaString(source_value.foreground_color),
       ",", color_utils::SkColorToRgbaString(source_value.background_color),
       ",", color_utils::SkColorToRgbaString(source_value.focus_ring_color),
       ",",
       color_utils::SkColorToRgbaString(
           source_value.close_button_focus_ring_color),
       "}"}));
}

// static
std::optional<TabStyle::TabColors> ui::metadata::TypeConverter<
    TabStyle::TabColors>::FromString(const std::u16string& source_value) {
  std::u16string trimmed_string;
  base::TrimString(source_value, u"{ }", &trimmed_string);
  std::u16string::const_iterator color_pos = trimmed_string.cbegin();
  const auto foreground_color = SkColorConverter::GetNextColor(
      color_pos, trimmed_string.cend(), color_pos);
  const auto background_color = SkColorConverter::GetNextColor(
      color_pos, trimmed_string.cend(), color_pos);
  const auto focus_ring_color = SkColorConverter::GetNextColor(
      color_pos, trimmed_string.cend(), color_pos);
  const auto close_button_focus_ring_color =
      SkColorConverter::GetNextColor(color_pos, trimmed_string.cend());
  return (foreground_color && background_color && focus_ring_color &&
          close_button_focus_ring_color)
             ? std::make_optional<TabStyle::TabColors>(
                   foreground_color.value(), background_color.value(),
                   focus_ring_color.value(),
                   close_button_focus_ring_color.value())
             : std::nullopt;
}

// static
ui::metadata::ValidStrings
ui::metadata::TypeConverter<TabStyle::TabColors>::GetValidStrings() {
  return ValidStrings();
}

// TabStyleViews ---------------------------------------------------------------

// static
std::unique_ptr<TabStyleViews> TabStyleViews::Create(
    std::unique_ptr<TabStyleViewDelegate> delegate,
    TabStripOrientation orientation) {
  if (orientation == TabStripOrientation::kVertical) {
    return std::make_unique<VerticalTabStyleViews>(std::move(delegate));
  }
  return std::make_unique<HorizontalTabStyleViews>(std::move(delegate));
}

TabStyleViews::TabStyleViews() : tab_style_(TabStyle::Get()) {}

TabStyleViews::~TabStyleViews() = default;
