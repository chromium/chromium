// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOLINT(build/header_guard)
// no-include-guard-because-multiply-included
#include "chrome/common/search/instant_types.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "components/favicon_base/favicon_types.h"
#include "components/ntp_tiles/ntp_tile_impression.h"
#include "components/ntp_tiles/tile_source.h"
#include "components/ntp_tiles/tile_title_source.h"
#include "components/ntp_tiles/tile_visual_type.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "ipc/ipc_message_macros.h"

IPC_ENUM_TRAITS_MAX_VALUE(OmniboxFocusState, OMNIBOX_FOCUS_STATE_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(OmniboxFocusChangeReason,
                          OMNIBOX_FOCUS_CHANGE_REASON_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(NTPLoggingEventType, NTP_EVENT_TYPE_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(NTPSuggestionsLoggingEventType,
                          NTPSuggestionsLoggingEventType::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(ntp_tiles::TileTitleSource,
                          ntp_tiles::TileTitleSource::LAST)

IPC_ENUM_TRAITS_MAX_VALUE(ntp_tiles::TileSource, ntp_tiles::TileSource::LAST)

IPC_ENUM_TRAITS_MAX_VALUE(ntp_tiles::TileVisualType, ntp_tiles::TILE_TYPE_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(ThemeBackgroundImageAlignment,
                          THEME_BKGRND_IMAGE_ALIGN_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(ThemeBackgroundImageTiling, THEME_BKGRND_IMAGE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(favicon_base::IconType, favicon_base::IconType::kMax)

IPC_STRUCT_TRAITS_BEGIN(ntp_tiles::NTPTileImpression)
  IPC_STRUCT_TRAITS_MEMBER(index)
  IPC_STRUCT_TRAITS_MEMBER(source)
  IPC_STRUCT_TRAITS_MEMBER(title_source)
  IPC_STRUCT_TRAITS_MEMBER(visual_type)
  IPC_STRUCT_TRAITS_MEMBER(icon_type)
  IPC_STRUCT_TRAITS_MEMBER(url_for_rappor)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(InstantMostVisitedItem)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(title)
  IPC_STRUCT_TRAITS_MEMBER(favicon)
  IPC_STRUCT_TRAITS_MEMBER(title_source)
  IPC_STRUCT_TRAITS_MEMBER(source)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(InstantMostVisitedInfo)
  IPC_STRUCT_TRAITS_MEMBER(items)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(NtpTheme)
  IPC_STRUCT_TRAITS_MEMBER(using_default_theme)
  IPC_STRUCT_TRAITS_MEMBER(custom_background_url)
  IPC_STRUCT_TRAITS_MEMBER(custom_background_attribution_line_1)
  IPC_STRUCT_TRAITS_MEMBER(custom_background_attribution_line_2)
  IPC_STRUCT_TRAITS_MEMBER(custom_background_attribution_action_url)
  IPC_STRUCT_TRAITS_MEMBER(collection_id)
  IPC_STRUCT_TRAITS_MEMBER(background_color)
  IPC_STRUCT_TRAITS_MEMBER(text_color)
  IPC_STRUCT_TRAITS_MEMBER(text_color_light)
  IPC_STRUCT_TRAITS_MEMBER(theme_id)
  IPC_STRUCT_TRAITS_MEMBER(image_horizontal_alignment)
  IPC_STRUCT_TRAITS_MEMBER(image_vertical_alignment)
  IPC_STRUCT_TRAITS_MEMBER(image_tiling)
  IPC_STRUCT_TRAITS_MEMBER(has_attribution)
  IPC_STRUCT_TRAITS_MEMBER(logo_alternate)
  IPC_STRUCT_TRAITS_MEMBER(has_theme_image)
IPC_STRUCT_TRAITS_END()
