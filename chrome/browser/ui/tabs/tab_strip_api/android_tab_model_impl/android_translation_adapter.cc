// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_translation_adapter.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_tab_strip_model_adapter.h"
#include "components/browser_apis/tab_strip/types/tab_states.h"
#include "components/favicon/content/content_favicon_util.h"
#include "components/tabs/public/tab_network_state.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "ui/base/models/image_model.h"

namespace tabs_api {

AndroidTranslationAdapter::AndroidTranslationAdapter(
    TabModel* model,
    AndroidTabStripModelAdapter& adapter)
    : model_(*model), adapter_(adapter) {}

AndroidTranslationAdapter::~AndroidTranslationAdapter() = default;

// static
Tab AndroidTranslationAdapter::BuildMojoTab(tabs::TabHandle handle,
                                            types::TabStates states) {
  auto* tab = handle.Get();
  CHECK(tab) << "invalid handle passed in for conversion";

  // TODO(crbug.com/445765534): Most of these are copied from the tsm converter
  // pkg. Maybe we should consolidate the two.
  Tab result;
  result.id = tabs_api::NodeId::FromTabHandle(handle);

  // Check `GetContents()` because the tab's `WebContents` may be unloaded (e.g.
  // lazily restored or discarded in the background on Android).
  if (auto* contents = tab->GetContents()) {
    const ui::ColorProvider& color_provider = contents->GetColorProvider();
    result.title = contents->GetTitle();

    auto image_model = ui::ImageModel::FromImage(
        favicon::GetTabFaviconMaybeDesaturatedOnError(contents));
    result.favicon = image_model.Rasterize(&color_provider);

    // TODO(crbug.com/445765534): favicon...
    content::NavigationEntry* entry =
        contents->GetController().GetLastCommittedEntry();
    const bool missing_navigation_entry = !entry || entry->IsInitialEntry();
    // In the case of reverted uncommitted navigations, there might not be a
    // valid NavigationEntry. In that case, show about:blank to match the
    // omnibox.
    result.url = missing_navigation_entry ? GURL(url::kAboutBlankURL)
                                          : contents->GetVisibleURL();
    result.network_state = tabs::TabNetworkStateForWebContents(contents);
    result.last_active = std::max(contents->GetLastActiveTimeTicks(),
                                  contents->GetLastInteractionTimeTicks());
  } else {
    result.title = tab->GetTitle();
    result.url = tab->GetURL();
    result.network_state = tabs::TabNetworkState::kNone;
    const base::TimeDelta time_since_active =
        base::Time::Now() - tab->GetLastActiveTime();
    result.last_active = base::TimeTicks::Now() - time_since_active;
  }

  result.is_active = states.is_active;
  result.is_selected = states.is_selected;
  result.is_blocked = tab->IsBlocked();

  return result;
}

base::expected<Tab, mojo_base::mojom::ErrorPtr>
AndroidTranslationAdapter::ToMojoTab(tabs::TabHandle handle) {
  return BuildMojoTab(handle, adapter_->GetTabStates(handle));
}

base::expected<mojom::DataPtr, mojo_base::mojom::ErrorPtr>
AndroidTranslationAdapter::ToMojoData(tabs::TabCollectionHandle handle) {
  auto* collection = handle.Get();
  CHECK(collection);

  switch (collection->type()) {
    case tabs::TabCollection::Type::TABSTRIP: {
      auto tab_strip = mojom::TabStrip::New();
      tab_strip->id = NodeId::FromTabCollectionHandle(handle);
      return mojom::Data::NewTabStrip(std::move(tab_strip));
    }
    case tabs::TabCollection::Type::PINNED: {
      auto pinned = mojom::PinnedTabs::New();
      pinned->id = NodeId::FromTabCollectionHandle(handle);
      return mojom::Data::NewPinnedTabs(std::move(pinned));
    }
    case tabs::TabCollection::Type::UNPINNED: {
      auto unpinned = mojom::UnpinnedTabs::New();
      unpinned->id = NodeId::FromTabCollectionHandle(handle);
      return mojom::Data::NewUnpinnedTabs(std::move(unpinned));
    }
    case tabs::TabCollection::Type::GROUP: {
      auto group_id = adapter_->FindGroupIdFor(handle);
      CHECK(group_id.has_value());

      auto group = mojom::TabGroup::New();
      group->id = NodeId::FromTabCollectionHandle(handle);

      auto visual_data = model_->GetTabGroupVisualData(group_id.value());
      if (visual_data.has_value()) {
        group->data = tab_groups::TabGroupVisualData(
            visual_data.value().title(), visual_data.value().color(),
            visual_data.value().is_collapsed());
      }
      return mojom::Data::NewTabGroup(std::move(group));
    }
    case tabs::TabCollection::Type::SPLIT: {
      auto split = mojom::SplitTab::New();
      split->id = NodeId::FromTabCollectionHandle(handle);
      // Split tabs are not fully supported on Android yet.
      split->data = split_tabs::SplitTabVisualData();
      return mojom::Data::NewSplitTab(std::move(split));
    }
  }

  NOTREACHED();
}

}  // namespace tabs_api
