// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOLINT(build/header_guard)
// no-include-guard-because-multiply-included
#include "chrome/common/search/instant_types.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "components/favicon_base/favicon_types.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "ipc/ipc_message_macros.h"

IPC_ENUM_TRAITS_MAX_VALUE(OmniboxFocusState, OMNIBOX_FOCUS_STATE_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(OmniboxFocusChangeReason,
                          OMNIBOX_FOCUS_CHANGE_REASON_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(NTPLoggingEventType, NTP_EVENT_TYPE_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(ThemeBackgroundImageAlignment,
                          THEME_BKGRND_IMAGE_ALIGN_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(ThemeBackgroundImageTiling, THEME_BKGRND_IMAGE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(favicon_base::IconType, favicon_base::IconType::kMax)

IPC_STRUCT_TRAITS_BEGIN(InstantMostVisitedItem)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(title)
  IPC_STRUCT_TRAITS_MEMBER(favicon)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(InstantMostVisitedInfo)
  IPC_STRUCT_TRAITS_MEMBER(items)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(NtpTheme)
  IPC_STRUCT_TRAITS_MEMBER(using_default_theme)
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
