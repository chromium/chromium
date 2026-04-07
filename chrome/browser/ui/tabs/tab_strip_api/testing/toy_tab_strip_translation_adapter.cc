// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip_translation_adapter.h"

#include "base/strings/string_number_conversions.h"
#include "components/browser_apis/tab_strip/types/node_id.h"

namespace tabs_api::testing {

ToyTabStripTranslationAdapter::ToyTabStripTranslationAdapter(
    ToyTabStrip* tab_strip)
    : tab_strip_(tab_strip) {}

ToyTabStripTranslationAdapter::~ToyTabStripTranslationAdapter() = default;

base::expected<mojom::TabPtr, mojo_base::mojom::ErrorPtr>
ToyTabStripTranslationAdapter::ToMojoTab(tabs::TabHandle handle) {
  auto maybe_tab = tab_strip_->GetToyTabFor(handle);
  if (!maybe_tab.has_value()) {
    auto err = mojo_base::mojom::Error::New(mojo_base::mojom::Code::kNotFound,
                                            "not found");
    return base::unexpected(std::move(err));
  }

  auto mojo_tab = mojom::Tab::New();
  mojo_tab->id = NodeId::FromTabHandle(handle);
  return mojo_tab;
}

base::expected<mojom::DataPtr, mojo_base::mojom::ErrorPtr>
ToyTabStripTranslationAdapter::ToMojoData(tabs::TabCollectionHandle handle) {
  auto* visual_data = tab_strip_->GetGroupVisualData(handle);
  if (visual_data) {
    auto mojo_group = mojom::TabGroup::New();
    mojo_group->id = NodeId::FromTabCollectionHandle(handle);
    mojo_group->data = *visual_data;
    return mojom::Data::NewTabGroup(std::move(mojo_group));
  }

  return base::unexpected(mojo_base::mojom::Error::New(
      mojo_base::mojom::Code::kInternal, "failed to find visual data"));
}

}  // namespace tabs_api::testing
