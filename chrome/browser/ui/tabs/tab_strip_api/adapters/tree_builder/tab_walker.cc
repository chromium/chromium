// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tree_builder/tab_walker.h"

#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_api/converters/tab_converters.h"

namespace tabs_api {

TabWalker::TabWalker(const TabStripModel* model, const tabs::TabInterface* tab)
    : model_(model), target_(tab) {}

mojom::TabContainerPtr TabWalker::Walk() {
  auto mojo_tab_container = tabs_api::mojom::TabContainer::New();
  auto idx = model_->GetIndexOfTab(target_);
  CHECK(idx != TabStripModel::kNoTab)
      << "tab disappeared while walking through the model";
  auto mojo_tab = tabs_api::converters::BuildMojoTab(
      target_->GetHandle(), TabRendererData::FromTabInModel(model_, idx));

  mojo_tab_container->tab = std::move(mojo_tab);
  return mojo_tab_container;
}

}  // namespace tabs_api
