// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/utilities/tab_strip_api_utilities.h"

namespace tabs_api::utils {

const tabs_api::NodeId& GetNodeId(const mojom::Data& data) {
  switch (data.which()) {
    case mojom::Data::Tag::kTabStrip:
      return data.get_tab_strip()->id;
    case mojom::Data::Tag::kPinnedTabs:
      return data.get_pinned_tabs()->id;
    case mojom::Data::Tag::kUnpinnedTabs:
      return data.get_unpinned_tabs()->id;
    case mojom::Data::Tag::kSplitTab:
      return data.get_split_tab()->id;
    case mojom::Data::Tag::kTabGroup:
      return data.get_tab_group()->id;
    case mojom::Data::Tag::kTab:
      return data.get_tab()->id;
  }
}

}  // namespace tabs_api::utils
