// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/search/instant_types.h"

SearchBoxTheme::SearchBoxTheme() = default;

SearchBoxTheme::SearchBoxTheme(const SearchBoxTheme& other) = default;

SearchBoxTheme::~SearchBoxTheme() = default;

bool SearchBoxTheme::operator==(const SearchBoxTheme& rhs) const {
  return bg == rhs.bg && icon == rhs.icon && placeholder == rhs.placeholder &&
         results_bg == rhs.results_bg &&
         results_bg_hovered == rhs.results_bg_hovered &&
         results_bg_selected == rhs.results_bg_selected &&
         results_dim == rhs.results_dim && results_text == rhs.results_text &&
         results_url == rhs.results_url && text == rhs.text;
}

NtpTheme::NtpTheme() = default;

NtpTheme::NtpTheme(const NtpTheme& other) = default;

NtpTheme::~NtpTheme() = default;

bool NtpTheme::operator==(const NtpTheme& rhs) const {
  return using_default_theme == rhs.using_default_theme &&
         using_dark_colors == rhs.using_dark_colors &&
         custom_background_url == rhs.custom_background_url &&
         custom_background_attribution_line_1 ==
             rhs.custom_background_attribution_line_1 &&
         custom_background_attribution_line_2 ==
             rhs.custom_background_attribution_line_2 &&
         custom_background_attribution_action_url ==
             rhs.custom_background_attribution_action_url &&
         collection_id == rhs.collection_id &&
         background_color == rhs.background_color &&
         text_color == rhs.text_color &&
         text_color_light == rhs.text_color_light && theme_id == rhs.theme_id &&
         image_horizontal_alignment == rhs.image_horizontal_alignment &&
         image_vertical_alignment == rhs.image_vertical_alignment &&
         image_tiling == rhs.image_tiling &&
         has_attribution == rhs.has_attribution &&
         logo_alternate == rhs.logo_alternate &&
         has_theme_image == rhs.has_theme_image &&
         theme_name == rhs.theme_name && color_id == rhs.color_id &&
         color_dark == rhs.color_dark && color_light == rhs.color_light &&
         color_picked == rhs.color_picked && logo_color == rhs.logo_color &&
         shortcut_color == rhs.shortcut_color && search_box == rhs.search_box;
}

InstantMostVisitedItem::InstantMostVisitedItem()
    : title_source(ntp_tiles::TileTitleSource::UNKNOWN),
      source(ntp_tiles::TileSource::TOP_SITES) {}

InstantMostVisitedItem::InstantMostVisitedItem(
    const InstantMostVisitedItem& other) = default;

InstantMostVisitedItem::~InstantMostVisitedItem() {}

InstantMostVisitedInfo::InstantMostVisitedInfo() = default;

InstantMostVisitedInfo::InstantMostVisitedInfo(
    const InstantMostVisitedInfo& other) = default;

InstantMostVisitedInfo::~InstantMostVisitedInfo() {}
