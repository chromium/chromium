// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_MOJOM_IME_MOJOM_TRAITS_H_
#define COMPONENTS_ARC_MOJOM_IME_MOJOM_TRAITS_H_

#include "components/arc/mojom/ime.mojom-shared.h"
#include "ui/base/ime/text_input_type.h"

namespace mojo {

template <>
struct EnumTraits<arc::mojom::TextInputType, ui::TextInputType> {
  using MojoType = arc::mojom::TextInputType;

  // The two enum types are similar, but intentionally made not identical.
  // We cannot force them to be in sync. If we do, updates in ui::TextInputType
  // must always be propagated to the mojom::TextInputType mojo definition in
  // ARC container side, which is in a different repository than Chromium.
  // We don't want such dependency.
  //
  // That's why we need a lengthy switch statement instead of static_cast
  // guarded by a static assert on the two enums to be in sync.

  static MojoType ToMojom(ui::TextInputType input) {
    switch (input) {
      case ui::TEXT_INPUT_TYPE_NONE:
        return MojoType::NONE;
      case ui::TEXT_INPUT_TYPE_TEXT:
        return MojoType::TEXT;
      case ui::TEXT_INPUT_TYPE_PASSWORD:
        return MojoType::PASSWORD;
      case ui::TEXT_INPUT_TYPE_SEARCH:
        return MojoType::SEARCH;
      case ui::TEXT_INPUT_TYPE_EMAIL:
        return MojoType::EMAIL;
      case ui::TEXT_INPUT_TYPE_NUMBER:
        return MojoType::NUMBER;
      case ui::TEXT_INPUT_TYPE_TELEPHONE:
        return MojoType::TELEPHONE;
      case ui::TEXT_INPUT_TYPE_URL:
        return MojoType::URL;
      case ui::TEXT_INPUT_TYPE_DATE:
        return MojoType::DATE;
      case ui::TEXT_INPUT_TYPE_DATE_TIME:
        return MojoType::DATETIME;
      case ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL:
        return MojoType::DATETIME;
      case ui::TEXT_INPUT_TYPE_MONTH:
        return MojoType::DATE;
      case ui::TEXT_INPUT_TYPE_TIME:
        return MojoType::TIME;
      case ui::TEXT_INPUT_TYPE_WEEK:
        return MojoType::DATE;
      case ui::TEXT_INPUT_TYPE_TEXT_AREA:
        return MojoType::TEXT;
      case ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE:
        return MojoType::TEXT;
      case ui::TEXT_INPUT_TYPE_DATE_TIME_FIELD:
        return MojoType::DATETIME;
    }
    NOTREACHED();
    return MojoType::TEXT;
  }

  static bool FromMojom(MojoType input, ui::TextInputType* out) {
    switch (input) {
      case MojoType::NONE:
        *out = ui::TEXT_INPUT_TYPE_NONE;
        return true;
      case MojoType::TEXT:
        *out = ui::TEXT_INPUT_TYPE_TEXT;
        return true;
      case MojoType::PASSWORD:
        *out = ui::TEXT_INPUT_TYPE_PASSWORD;
        return true;
      case MojoType::SEARCH:
        *out = ui::TEXT_INPUT_TYPE_SEARCH;
        return true;
      case MojoType::EMAIL:
        *out = ui::TEXT_INPUT_TYPE_EMAIL;
        return true;
      case MojoType::NUMBER:
        *out = ui::TEXT_INPUT_TYPE_NUMBER;
        return true;
      case MojoType::TELEPHONE:
        *out = ui::TEXT_INPUT_TYPE_TELEPHONE;
        return true;
      case MojoType::URL:
        *out = ui::TEXT_INPUT_TYPE_URL;
        return true;
      case MojoType::DATE:
        *out = ui::TEXT_INPUT_TYPE_DATE;
        return true;
      case MojoType::TIME:
        *out = ui::TEXT_INPUT_TYPE_TIME;
        return true;
      case MojoType::DATETIME:
        *out = ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

}  // namespace mojo

#endif  // COMPONENTS_ARC_MOJOM_IME_MOJOM_TRAITS_H_
