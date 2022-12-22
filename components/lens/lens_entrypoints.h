// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_ENTRYPOINTS_H_
#define COMPONENTS_LENS_LENS_ENTRYPOINTS_H_

namespace lens {

// Lens entry points for LWD.
enum EntryPoint {
  CHROME_OPEN_NEW_TAB_SIDE_PANEL,
  CHROME_REGION_SEARCH_MENU_ITEM,
  CHROME_SEARCH_WITH_GOOGLE_LENS_CONTEXT_MENU_ITEM,
  CHROME_FULLSCREEN_SEARCH_MENU_ITEM,
  UNKNOWN
};

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_ENTRYPOINTS_H_
