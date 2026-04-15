// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/utilities/tab_strip_api_utilities.h"

#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter.h"

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
    case mojom::Data::Tag::kWindow:
      return data.get_window()->id;
  }
}

base::expected<tab_groups::TabGroupId, mojo_base::mojom::ErrorPtr>
GetTabGroupId(TabStripModelAdapter& adapter, const NodeId& node_id) {
  auto collection_handle = node_id.ToTabCollectionHandle();
  if (!collection_handle.has_value()) {
    return base::unexpected(
        mojo_base::mojom::Error::New(mojo_base::mojom::Code::kInvalidArgument,
                                     "only collection ids accepted"));
  }

  auto group_id = adapter.FindGroupIdFor(collection_handle.value());
  if (!group_id.has_value()) {
    return base::unexpected(
        mojo_base::mojom::Error::New(mojo_base::mojom::Code::kNotFound,
                                     "group with the specified ID not found."));
  }
  return group_id.value();
}

base::expected<tab_groups::TabGroupVisualData, mojo_base::mojom::ErrorPtr>
MergeTabGroupVisualData(const tab_groups::TabGroupVisualData& current,
                        const tab_groups::TabGroupVisualData& incoming,
                        const std::optional<std::vector<std::string>>& mask) {
  if (!mask || mask->empty()) {
    return incoming;
  }

  std::u16string title = current.title();
  tab_groups::TabGroupColorId color = current.color();
  bool is_collapsed = current.is_collapsed();

  for (const auto& field : *mask) {
    if (field == kTitleField) {
      title = incoming.title();
    } else if (field == kColorField) {
      color = incoming.color();
    } else if (field == kIsCollapsedField) {
      is_collapsed = incoming.is_collapsed();
    } else {
      return base::unexpected(mojo_base::mojom::Error::New(
          mojo_base::mojom::Code::kInvalidArgument,
          "Invalid field in update mask: " + field));
    }
  }

  return tab_groups::TabGroupVisualData(title, color, is_collapsed);
}

}  // namespace tabs_api::utils
