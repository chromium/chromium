// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/converters/tab_converters.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/tabs/tab_utils.h"

namespace tabs_api::converters {

tabs_api::mojom::TabPtr BuildMojoTab(tabs::TabHandle handle,
                                     const TabRendererData& data) {
  auto result = tabs_api::mojom::Tab::New();

  result->id = tabs_api::NodeId(tabs_api::NodeId::Type::kContent,
                               base::NumberToString(handle.raw_value()));
  result->title = base::UTF16ToUTF8(data.title);
  // TODO(crbug.com/414630734). Integrate the favicon_url after it is
  // typemapped.
  result->url = data.visible_url;
  result->network_state = data.network_state;
  if (handle.Get() != nullptr) {
    for (const auto alert_state : GetTabAlertStatesForTab(handle.Get())) {
      result->alert_states.push_back(alert_state);
    }
  }

  return result;
}

tabs_api::mojom::TabCollectionPtr BuildMojoTabCollection(
    tabs::TabCollectionHandle handle,
    tabs::TabCollection::Type collection_type) {
  auto tab_collection = tabs_api::mojom::TabCollection::New();
  tab_collection->id =
      tabs_api::NodeId(tabs_api::NodeId::Type::kCollection,
                      base::NumberToString(handle.raw_value()));
  tab_collection->collection_type =
      tabs_api::mojom::TabCollection::CollectionType::kUnknown;
  switch (collection_type) {
    case tabs::TabCollection::Type::TABSTRIP:
      tab_collection->collection_type =
          tabs_api::mojom::TabCollection::CollectionType::kTabStrip;
      break;
    case tabs::TabCollection::Type::PINNED:
      tab_collection->collection_type =
          tabs_api::mojom::TabCollection::CollectionType::kPinned;
      break;
    case tabs::TabCollection::Type::UNPINNED:
      tab_collection->collection_type =
          tabs_api::mojom::TabCollection::CollectionType::kUnpinned;
      break;
    case tabs::TabCollection::Type::GROUP:
      tab_collection->collection_type =
          tabs_api::mojom::TabCollection::CollectionType::kTabGroup;
      break;
    case tabs::TabCollection::Type::SPLIT:
      tab_collection->collection_type =
          tabs_api::mojom::TabCollection::CollectionType::kSplitTab;
      break;
  }
  return tab_collection;
}

tabs_api::mojom::TabGroupVisualDataPtr BuildMojoTabGroupVisualData(
    const tab_groups::TabGroupVisualData& visual_data) {
  auto tab_group_visual_data = tabs_api::mojom::TabGroupVisualData::New();
  tab_group_visual_data->title = base::UTF16ToUTF8(visual_data.title());
  tab_group_visual_data->color = visual_data.color();
  tab_group_visual_data->is_collapsed = visual_data.is_collapsed();
  return tab_group_visual_data;
}

}  // namespace tabs_api::converters
