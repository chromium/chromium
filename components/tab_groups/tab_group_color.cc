// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tab_groups/tab_group_color.h"

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/skia/include/utils/SkRandom.h"
#include "ui/base/l10n/l10n_util.h"

namespace tab_groups {

const ColorLabelMap& GetTabGroupColorLabelMap() {
  static const base::NoDestructor<ColorLabelMap> kTabGroupColors(
      {{TabGroupColorId::kGrey,
        l10n_util::GetStringUTF16(IDS_TAB_GROUP_COLOR_GREY)},
       {TabGroupColorId::kBlue,
        l10n_util::GetStringUTF16(IDS_TAB_GROUP_COLOR_BLUE)},
       {TabGroupColorId::kRed,
        l10n_util::GetStringUTF16(IDS_TAB_GROUP_COLOR_RED)},
       {TabGroupColorId::kYellow,
        l10n_util::GetStringUTF16(IDS_TAB_GROUP_COLOR_YELLOW)},
       {TabGroupColorId::kGreen,
        l10n_util::GetStringUTF16(IDS_TAB_GROUP_COLOR_GREEN)},
       {TabGroupColorId::kPink,
        l10n_util::GetStringUTF16(IDS_TAB_GROUP_COLOR_PINK)},
       {TabGroupColorId::kPurple,
        l10n_util::GetStringUTF16(IDS_TAB_GROUP_COLOR_PURPLE)},
       {TabGroupColorId::kCyan,
        l10n_util::GetStringUTF16(IDS_TAB_GROUP_COLOR_CYAN)}});
  return *kTabGroupColors;
}

}  // namespace tab_groups
