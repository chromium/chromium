// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COLOR_CHROME_COLOR_PROVIDER_UTILS_H_
#define CHROME_BROWSER_UI_COLOR_CHROME_COLOR_PROVIDER_UTILS_H_

#include "base/strings/string_piece.h"
#include "ui/color/color_id.h"

// Converts ColorId if |color_id| is in CHROME_COLOR_IDS.
base::StringPiece ChromeColorIdName(ui::ColorId color_id);

#endif  // CHROME_BROWSER_UI_COLOR_CHROME_COLOR_PROVIDER_UTILS_H_
