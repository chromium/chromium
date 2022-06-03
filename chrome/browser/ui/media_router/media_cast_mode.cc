// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/media_cast_mode.h"

#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace media_router {

std::string MediaCastModeToDescription(MediaCastMode mode,
                                       const std::string& host) {
  switch (mode) {
    case MediaCastMode::PRESENTATION:
      return l10n_util::GetStringFUTF8(IDS_MEDIA_ROUTER_PRESENTATION_CAST_MODE,
                                       base::UTF8ToUTF16(host));
    case MediaCastMode::TAB_MIRROR:
      return l10n_util::GetStringUTF8(IDS_MEDIA_ROUTER_TAB_MIRROR_CAST_MODE);
    case MediaCastMode::DESKTOP_MIRROR:
      return l10n_util::GetStringUTF8(
          IDS_MEDIA_ROUTER_DESKTOP_MIRROR_CAST_MODE);
    case MediaCastMode::LOCAL_FILE:
      return l10n_util::GetStringUTF8(IDS_MEDIA_ROUTER_LOCAL_FILE_CAST_MODE);
    default:
      NOTREACHED();
      return "";
  }
}

bool IsValidCastModeNum(int cast_mode_num) {
  switch (cast_mode_num) {
    case MediaCastMode::PRESENTATION:
    case MediaCastMode::TAB_MIRROR:
    case MediaCastMode::DESKTOP_MIRROR:
    case MediaCastMode::LOCAL_FILE:
      return true;
    default:
      return false;
  }
}

}  // namespace media_router
