// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_UTILITIES_TAB_ID_UTILS_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_UTILITIES_TAB_ID_UTILS_H_

#include "base/types/expected.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "components/browser_apis/tab_strip/types/path.h"
#include "mojo/public/mojom/base/error.mojom.h"

namespace tabs_api::utils {

base::expected<void, mojo_base::mojom::ErrorPtr>
CheckPath(const Path& path, const NodeId& window_id, const NodeId& tab_strip);

base::expected<void, mojo_base::mojom::ErrorPtr> CheckIsContentType(
    const NodeId& node_id);
base::expected<int32_t, mojo_base::mojom::ErrorPtr> GetNativeTabId(
    const NodeId& node_id);

// Gets the native id for a content tab. Error if the id is not for a content
// type.
base::expected<int32_t, mojo_base::mojom::ErrorPtr> GetContentNativeTabId(
    const NodeId& node_id);

}  // namespace tabs_api::utils

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_UTILITIES_TAB_ID_UTILS_H_
