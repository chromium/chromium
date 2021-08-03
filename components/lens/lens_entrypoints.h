// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_ENTRYPOINTS_H_
#define COMPONENTS_LENS_LENS_ENTRYPOINTS_H_

#include <string>

namespace lens {

// Lens entry points for LWD.
enum EntryPoint {
  CHROME_REGION_SEARCH_MENU_ITEM,
  CHROME_SEARCH_WITH_GOOGLE_LENS_CONTEXT_MENU_ITEM,
  UNKNOWN
};

extern std::string GetQueryParameterFromEntryPoint(EntryPoint entry_point);

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_ENTRYPOINTS_H_
