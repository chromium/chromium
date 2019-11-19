// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/page_break_constants.h"

#include "base/stl_util.h"

namespace app_list {

// Generated as:
// crx_file::id_util::GenerateId("org.chromium.default_page_break_1").
const char kDefaultPageBreak1[] = "fdipebbchlhkdibfjgbfalhceahammim";

const char* const kDefaultPageBreakAppIds[] = {
    kDefaultPageBreak1,
};

const size_t kDefaultPageBreakAppIdsLength =
    base::size(kDefaultPageBreakAppIds);

// Returns true if |item_id| is of a default-installed page break item.
bool IsDefaultPageBreakItem(const std::string& item_id) {
  return base::Contains(kDefaultPageBreakAppIds, item_id);
}

}  // namespace app_list
