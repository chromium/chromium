// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_STRIP_INTERNALS_TAB_STRIP_INTERNALS_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_STRIP_INTERNALS_TAB_STRIP_INTERNALS_UTIL_H_

#include <string>

#include "chrome/browser/ui/webui/tab_strip_internals/tab_strip_internals.mojom.h"

class TabStripModel;

namespace tab_strip_internals {

// Utility to create a NodeId.
mojom::NodeIdPtr MakeNodeId(const std::string& id, mojom::NodeId::Type type);

// Recursively builds the tab collection tree for the given `model`.
mojom::NodePtr BuildTabCollectionTree(const TabStripModel* model);

// Builds a selection model for the given `model`.
mojom::SelectionModelPtr BuildSelectionModel(const TabStripModel* model);

}  // namespace tab_strip_internals

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_STRIP_INTERNALS_TAB_STRIP_INTERNALS_UTIL_H_
