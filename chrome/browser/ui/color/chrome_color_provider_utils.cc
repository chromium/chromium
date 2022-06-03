// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/chrome_color_provider_utils.h"

#include "base/containers/fixed_flat_map.h"
#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/ui/color/chrome_color_id.h"

#include "ui/color/color_id_map_macros.inc"

base::StringPiece ChromeColorIdName(ui::ColorId color_id) {
  static constexpr const auto color_id_map =
      base::MakeFixedFlatMap<ui::ColorId, const char*>({CHROME_COLOR_IDS});
  auto* i = color_id_map.find(color_id);
  if (i != color_id_map.cend())
    return i->second;
  NOTREACHED();
  return "<invalid>";
}

#include "ui/color/color_id_map_macros.inc"
