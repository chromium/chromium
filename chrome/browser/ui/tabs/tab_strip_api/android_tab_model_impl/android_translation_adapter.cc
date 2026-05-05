// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_translation_adapter.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_tab_strip_model_adapter.h"
#include "components/tabs/public/tab_network_state.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/models/image_model.h"

namespace tabs_api {

AndroidTranslationAdapter::AndroidTranslationAdapter(
    TabModel* model,
    AndroidTabStripModelAdapter& adapter)
    : model_(*model), adapter_(adapter) {}

AndroidTranslationAdapter::~AndroidTranslationAdapter() = default;

base::expected<mojom::TabPtr, mojo_base::mojom::ErrorPtr>
AndroidTranslationAdapter::ToMojoTab(tabs::TabHandle handle) {
  auto* tab = handle.Get();
  CHECK(tab) << "invalid handle passed in for conversion";
  const ui::ColorProvider& color_provider = adapter_->GetColorProvider();

  // TODO(crbug.com/445765534): Most of these are copied from the tsm converter
  // pkg. Maybe we should consolidate the two.
  auto result = tabs_api::mojom::Tab::New();
  auto* contents = tab->GetContents();
  // TODO(crbug.com/498763314): This is not strictly guaranteed in android,
  // because web contents is lazily loaded.
  CHECK(contents);

  result->id = tabs_api::NodeId::FromTabHandle(handle);
  result->title = base::UTF16ToUTF8(contents->GetTitle());

  auto image_model =
      ui::ImageModel::FromImage(favicon::TabFaviconFromWebContents(contents));
  result->favicon = image_model.Rasterize(&color_provider);

  // TODO(crbug.com/445765534): favicon...
  content::NavigationEntry* entry =
      contents->GetController().GetLastCommittedEntry();
  const bool missing_navigation_entry = !entry || entry->IsInitialEntry();
  // In the case of reverted uncommitted navigations, there might not be a valid
  // NavigationEntry. In that case, show about:blank to match the omnibox.
  auto url = missing_navigation_entry ? GURL(url::kAboutBlankURL)
                                      : contents->GetVisibleURL();
  result->url = url;
  result->network_state = tabs::TabNetworkStateForWebContents(contents);

  auto states = adapter_->GetTabStates(handle);
  result->is_active = states.is_active;
  result->is_selected = states.is_selected;
  result->is_blocked = tab->IsBlocked();

  result->last_active_time_ticks =
      std::max(contents->GetLastInteractionTimeTicks(),
               contents->GetLastActiveTimeTicks());
  result->last_active_elapsed_text = base::UTF16ToUTF8(ui::TimeFormat::Simple(
      ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_SHORT,
      base::TimeTicks::Now() - result->last_active_time_ticks));
  return result;
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
