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
        NOTREACHED() << "kNumEntries is not a supported color enum.";
    }
  }

  static TabGroupColorId FromMojom(MojoTabGroupColorId input) {
    switch (input) {
      case MojoTabGroupColorId::kGrey:
        return TabGroupColorId::kGrey;
      case MojoTabGroupColorId::kBlue:
        return TabGroupColorId::kBlue;
      case MojoTabGroupColorId::kRed:
        return TabGroupColorId::kRed;
      case MojoTabGroupColorId::kYellow:
        return TabGroupColorId::kYellow;
      case MojoTabGroupColorId::kGreen:
        return TabGroupColorId::kGreen;
      case MojoTabGroupColorId::kPink:
        return TabGroupColorId::kPink;
      case MojoTabGroupColorId::kPurple:
        return TabGroupColorId::kPurple;
      case MojoTabGroupColorId::kCyan:
        return TabGroupColorId::kCyan;
      case MojoTabGroupColorId::kOrange:
        return TabGroupColorId::kOrange;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // COMPONENTS_TAB_GROUPS_PUBLIC_MOJOM_TAB_GROUPS_MOJOM_TRAITS_H_
