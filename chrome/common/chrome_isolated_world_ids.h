// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_ISOLATED_WORLD_IDS_H_
#define CHROME_COMMON_CHROME_ISOLATED_WORLD_IDS_H_

#include "build/build_config.h"
#include "content/public/common/isolated_world_ids.h"

enum ChromeIsolatedWorldIDs {
  // Isolated world ID for Chrome Translate.
  ISOLATED_WORLD_ID_TRANSLATE = content::ISOLATED_WORLD_ID_CONTENT_END + 1,

  // Isolated world ID for internal Chrome features.
  ISOLATED_WORLD_ID_CHROME_INTERNAL,

#if defined(OS_MAC)
  // Isolated world ID for AppleScript.
  ISOLATED_WORLD_ID_APPLESCRIPT,
#endif  // defined(OS_MAC)

  // Numbers for isolated worlds for extensions are set in
  // extensions/renderer/script_injection.cc, and are are greater than or equal
  // to this number.
  ISOLATED_WORLD_ID_EXTENSIONS
};

#endif  // CHROME_COMMON_CHROME_ISOLATED_WORLD_IDS_H_
