// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_BOOKMARK_BAR_NODE_ID_CONVERSIONS_H_
#define CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_BOOKMARK_BAR_NODE_ID_CONVERSIONS_H_

#include "chrome/browser/bookmarks/bookmark_node_types.h"
#include "components/browser_apis/bookmarks/bookmarks_api.mojom-forward.h"
#include "components/browser_apis/ui_controllers/bookmark_bar/bookmark_bar_api.mojom.h"

// Converts chrome's bookmark node IDs to the bookmark bar UI controller's.
bookmark_bar_api::mojom::NodeIdPtr ToMojoNodeId(
    const bookmarks::BookmarkNodeId& id);

// Converts a Browser API permanent folder type to chrome's. `type` must not be
// kUnknown.
bookmarks::PermanentFolderType FromMojoPermanentFolderType(
    bookmarks_api::mojom::PermanentFolderType type);

#endif  // CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_BOOKMARK_BAR_NODE_ID_CONVERSIONS_H_
