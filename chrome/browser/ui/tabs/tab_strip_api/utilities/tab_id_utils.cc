// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/utilities/tab_id_utils.h"

#include "base/strings/string_number_conversions.h"
#include "base/types/expected_macros.h"

namespace tabs_api::utils {

base::expected<void, mojo_base::mojom::ErrorPtr>
CheckPath(const Path& path, const NodeId& window_id, const NodeId& tab_strip) {
  if (path.components().empty()) {
    return base::ok();
  }

  if (path.components().size() < 2 || path.components()[0] != window_id ||
      path.components()[1] != tab_strip) {
    // TODO(crbug.com/439964658) Add path as a str in the error.
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInvalidArgument, "invalid absolute path"));
  }

  return base::ok();
}

base::expected<void, mojo_base::mojom::ErrorPtr> CheckIsContentType(
    const NodeId& node_id) {
  if (node_id.Type() != tabs_api::NodeId::Type::kContent) {
    return base::unexpected(
        mojo_base::mojom::Error::New(mojo_base::mojom::Code::kInvalidArgument,
                                     "only tab content ids accepted"));
  }
  return base::ok();
}

base::expected<int32_t, mojo_base::mojom::ErrorPtr> GetNativeTabId(
    const NodeId& node_id) {
  int32_t tab_id;
  if (!base::StringToInt(node_id.Id(), &tab_id)) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInvalidArgument, "invalid tab id provided"));
  }
  return tab_id;
}

base::expected<int32_t, mojo_base::mojom::ErrorPtr> GetContentNativeTabId(
    const NodeId& node_id) {
  RETURN_IF_ERROR(CheckIsContentType(node_id));
  ASSIGN_OR_RETURN(auto native, GetNativeTabId(node_id));
  return native;
}

}  // namespace tabs_api::utils
