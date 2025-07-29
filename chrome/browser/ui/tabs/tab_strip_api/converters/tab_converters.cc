// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/converters/tab_converters.h"

#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_group_tab_collection.h"

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
    const tabs::TabCollection* collection) {
  CHECK(collection);
  auto node_id = tabs_api::NodeId(
      tabs_api::NodeId::Type::kCollection,
      base::NumberToString(collection->GetHandle().raw_value()));
  switch (collection->type()) {
    case tabs::TabCollection::Type::TABSTRIP: {
      auto mojo_tab_strip = tabs_api::mojom::TabStrip::New();
      mojo_tab_strip->id = node_id;
      return tabs_api::mojom::TabCollection::NewTabStrip(
          std::move(mojo_tab_strip));
    }
    case tabs::TabCollection::Type::PINNED: {
      auto mojo_pinned_tabs = tabs_api::mojom::PinnedTabs::New();
      mojo_pinned_tabs->id = node_id;
      return tabs_api::mojom::TabCollection::NewPinnedTabs(
          std::move(mojo_pinned_tabs));
    }
    case tabs::TabCollection::Type::UNPINNED: {
      auto mojo_unpinned_tabs = tabs_api::mojom::UnpinnedTabs::New();
      mojo_unpinned_tabs->id = node_id;
      return tabs_api::mojom::TabCollection::NewUnpinnedTabs(
          std::move(mojo_unpinned_tabs));
    }
    case tabs::TabCollection::Type::GROUP: {
      auto mojo_tab_group = tabs_api::mojom::TabGroup::New();
      mojo_tab_group->id = node_id;
      const tabs::TabGroupTabCollection* group_collection =
          static_cast<const tabs::TabGroupTabCollection*>(collection);
      const TabGroup* tab_group = group_collection->GetTabGroup();
      CHECK(tab_group);
      mojo_tab_group->data = *tab_group->visual_data();
      return tabs_api::mojom::TabCollection::NewTabGroup(
          std::move(mojo_tab_group));
    }
    case tabs::TabCollection::Type::SPLIT: {
      // TODO(crbug.com/421933194): Add SplitTab.
      NOTIMPLEMENTED();
    }
  }
  NOTREACHED();
}

}  // namespace tabs_api::converters
