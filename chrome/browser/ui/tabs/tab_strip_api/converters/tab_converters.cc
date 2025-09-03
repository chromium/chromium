// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/converters/tab_converters.h"

#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/split_tab_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs_api::converters {

tabs_api::mojom::TabPtr BuildMojoTab(tabs::TabHandle handle,
                                     const TabRendererData& data,
                                     const ui::ColorProvider& color_provider,
                                     const TabStates& states) {
  auto result = tabs_api::mojom::Tab::New();

  result->id = tabs_api::NodeId(tabs_api::NodeId::Type::kContent,
                               base::NumberToString(handle.raw_value()));
  result->title = base::UTF16ToUTF8(data.title);
  result->favicon = data.favicon.Rasterize(&color_provider);
  result->url = data.visible_url;
  result->network_state = data.network_state;
  if (handle.Get() != nullptr) {
    result->alert_states =
        tabs::TabAlertController::From(handle.Get())->GetAllActiveAlerts();
  }

  result->is_active = states.is_active;
  result->is_selected = states.is_selected;
  result->is_blocked = data.blocked;

  return result;
}

tabs_api::mojom::DataPtr BuildMojoTabCollectionData(
    tabs::TabCollectionHandle handle) {
  const tabs::TabCollection* collection = handle.Get();
  CHECK(collection);
  auto node_id = tabs_api::NodeId(
      tabs_api::NodeId::Type::kCollection,
      base::NumberToString(collection->GetHandle().raw_value()));
  switch (collection->type()) {
    case tabs::TabCollection::Type::TABSTRIP: {
      auto mojo_tab_strip = tabs_api::mojom::TabStrip::New();
      mojo_tab_strip->id = node_id;
      return tabs_api::mojom::Data::NewTabStrip(std::move(mojo_tab_strip));
    }
    case tabs::TabCollection::Type::PINNED: {
      auto mojo_pinned_tabs = tabs_api::mojom::PinnedTabs::New();
      mojo_pinned_tabs->id = node_id;
      return tabs_api::mojom::Data::NewPinnedTabs(std::move(mojo_pinned_tabs));
    }
    case tabs::TabCollection::Type::UNPINNED: {
      auto mojo_unpinned_tabs = tabs_api::mojom::UnpinnedTabs::New();
      mojo_unpinned_tabs->id = node_id;
      return tabs_api::mojom::Data::NewUnpinnedTabs(
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
      return tabs_api::mojom::Data::NewTabGroup(std::move(mojo_tab_group));
    }
    case tabs::TabCollection::Type::SPLIT: {
      auto mojo_split_tab = tabs_api::mojom::SplitTab::New();
      mojo_split_tab->id = node_id;
      const tabs::SplitTabCollection* split_collection =
          static_cast<const tabs::SplitTabCollection*>(collection);
      split_tabs::SplitTabData* split_data = split_collection->data();
      CHECK(split_data);
      split_tabs::SplitTabVisualData* visual_data = split_data->visual_data();
      CHECK(visual_data);
      mojo_split_tab->data = *visual_data;
      return tabs_api::mojom::Data::NewSplitTab(std::move(mojo_split_tab));
    }
  }
  NOTREACHED();
}

}  // namespace tabs_api::converters
