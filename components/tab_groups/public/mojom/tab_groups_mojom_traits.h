// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TAB_GROUPS_PUBLIC_MOJOM_TAB_GROUPS_MOJOM_TRAITS_H_
#define COMPONENTS_TAB_GROUPS_PUBLIC_MOJOM_TAB_GROUPS_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "components/tab_groups/public/mojom/tab_group_types.mojom.h"
#include "components/tab_groups/tab_group_color.h"

namespace mojo {

template <>
struct EnumTraits<tab_groups::mojom::Color, tab_groups::TabGroupColorId> {
  using TabGroupColorId = tab_groups::TabGroupColorId;
  using MojoTabGroupColorId = tab_groups::mojom::Color;

  static MojoTabGroupColorId ToMojom(TabGroupColorId input) {
    switch (input) {
      case TabGroupColorId::kGrey:
        return MojoTabGroupColorId::kGrey;
      case TabGroupColorId::kBlue:
        return MojoTabGroupColorId::kBlue;
      case TabGroupColorId::kRed:
        return MojoTabGroupColorId::kRed;
      case TabGroupColorId::kYellow:
        return MojoTabGroupColorId::kYellow;
      case TabGroupColorId::kGreen:
        return MojoTabGroupColorId::kGreen;
      case TabGroupColorId::kPink:
        return MojoTabGroupColorId::kPink;
      case TabGroupColorId::kPurple:
        return MojoTabGroupColorId::kPurple;
      case TabGroupColorId::kCyan:
        return MojoTabGroupColorId::kCyan;
      case TabGroupColorId::kOrange:
        return MojoTabGroupColorId::kOrange;
      case TabGroupColorId::kNumEntries:
        NOTREACHED_IN_MIGRATION()
            << "kNumEntries is not a supported color enum.";
        return MojoTabGroupColorId::kGrey;
    }
  }

  static bool FromMojom(MojoTabGroupColorId input, TabGroupColorId* out) {
    switch (input) {
      case MojoTabGroupColorId::kGrey:
        *out = TabGroupColorId::kGrey;
        return true;
      case MojoTabGroupColorId::kBlue:
        *out = TabGroupColorId::kBlue;
        return true;
      case MojoTabGroupColorId::kRed:
        *out = TabGroupColorId::kRed;
        return true;
      case MojoTabGroupColorId::kYellow:
        *out = TabGroupColorId::kYellow;
        return true;
      case MojoTabGroupColorId::kGreen:
        *out = TabGroupColorId::kGreen;
        return true;
      case MojoTabGroupColorId::kPink:
        *out = TabGroupColorId::kPink;
        return true;
      case MojoTabGroupColorId::kPurple:
        *out = TabGroupColorId::kPurple;
        return true;
      case MojoTabGroupColorId::kCyan:
        *out = TabGroupColorId::kCyan;
        return true;
      case MojoTabGroupColorId::kOrange:
        *out = TabGroupColorId::kOrange;
        return true;
    }
    NOTREACHED_IN_MIGRATION();
    return false;
  }
};

}  // namespace mojo

#endif  // COMPONENTS_TAB_GROUPS_PUBLIC_MOJOM_TAB_GROUPS_MOJOM_TRAITS_H_
