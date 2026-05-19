// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_model_impl/converters/tab_converters.h"

#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/tab_alert.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tabs/public/tab_network_state.h"
#include "content/public/browser/web_contents.h"

namespace tabs_api::converters {

tabs_api::mojom::TabFieldMaskPtr BuildTabFieldMask(TabChangeType type) {
  auto mask = tabs_api::mojom::TabFieldMask::New();
  // These mappings should match the signals dispatched from TabStripModel.
  // Note that kAll specifically excludes the other TabChangeTypes. It acts as
  // catch-all for the non granular updates.
  switch (type) {
    case TabChangeType::kAll:
      mask->title = true;
      mask->url = true;
      mask->favicon = true;
      mask->alert_states = true;
      mask->last_active = true;
      break;
    case TabChangeType::kLoadingOnly:
      mask->network_state = true;
      break;
    case TabChangeType::kAttentionOnly:
      // AttentionOnly affects "needs_attention" which is not yet exposed.
      break;
    case TabChangeType::kBlockedOnly:
      mask->is_blocked = true;
      break;
  }
  return mask;
}

tabs_api::mojom::TabFieldMaskPtr BuildTabFieldMaskForSelection(bool active,
                                                               bool selected) {
  auto mask = tabs_api::mojom::TabFieldMask::New();
  mask->is_active = active;
  mask->is_selected = selected;
  return mask;
}

tabs_api::mojom::TabPtr BuildMojoTab(tabs::TabInterface* tab,
                                     const ui::ColorProvider& color_provider,
                                     const types::TabStates& states) {
  auto result = tabs_api::mojom::Tab::New();
  TabUIHelper* tab_ui_helper = TabUIHelper::From(tab);
  CHECK(tab_ui_helper);

  result->id =
      tabs_api::NodeId(tabs_api::NodeId::Type::kContent,
                       base::NumberToString(tab->GetHandle().raw_value()));
  result->title = base::UTF16ToUTF8(tab_ui_helper->GetTitle());
  result->favicon = tab_ui_helper->GetFavicon().Rasterize(&color_provider);
  result->url = tab_ui_helper->GetVisibleURL();
  result->network_state = tab_ui_helper->GetTabNetworkState();
  if (tab->GetHandle().Get() != nullptr) {
    result->alert_states =
        tabs::TabAlertController::From(tab->GetHandle().Get())
            ->GetAllActiveAlerts();
  }

  result->is_active = states.is_active;
  result->is_selected = states.is_selected;
  result->is_blocked = tab->IsBlocked();
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
