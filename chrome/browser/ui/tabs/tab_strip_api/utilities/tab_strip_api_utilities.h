// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_UTILITIES_TAB_STRIP_API_UTILITIES_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_UTILITIES_TAB_STRIP_API_UTILITIES_H_

#include "base/types/expected.h"
#include "components/browser_apis/tab_strip/tab_strip_api_types.mojom.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "components/tab_groups/tab_group_id.h"
#include "mojo/public/mojom/base/error.mojom.h"

namespace tabs_api {
class TabStripModelAdapter;
}

// Helper functions for clients of the TabStripService API.
namespace tabs_api::utils {

// Returns the NodeId from any variant of the mojom::Data union.
const tabs_api::NodeId& GetNodeId(const mojom::Data& data);

// Returns the tab group id for a given node id.
base::expected<tab_groups::TabGroupId, mojo_base::mojom::ErrorPtr>
GetTabGroupId(TabStripModelAdapter& adapter, const NodeId& node_id);

}  // namespace tabs_api::utils

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_UTILITIES_TAB_STRIP_API_UTILITIES_H_
