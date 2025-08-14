// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tree_builder/tab_walker.h"

#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_api/converters/tab_converters.h"
#include "content/public/browser/web_contents.h"

namespace tabs_api {

TabWalker::TabWalker(const TabStripModel* model, const tabs::TabInterface* tab)
    : model_(model), target_(tab) {}

mojom::ContainerPtr TabWalker::Walk() {
  auto idx = model_->GetIndexOfTab(target_);
  CHECK(idx != TabStripModel::kNoTab)
      << "tab disappeared while walking through the model";

  content::WebContents* contents = model_->GetWebContentsAt(idx);
  CHECK(contents);
  const ui::ColorProvider& provider = contents->GetColorProvider();
  mojom::TabPtr mojo_tab = converters::BuildMojoTab(
      target_->GetHandle(), TabRendererData::FromTabInModel(model_, idx),
      // TODO(crbug.com/438632110): this is dup code with the adapter. See if
      // we can combine state computation.
      provider,
      {
          .is_active = target_->IsActivated(),
          .is_selected = target_->IsSelected(),
      });
  auto node = tabs_api::mojom::Container::New();
  node->data = mojom::Data::NewTab(std::move(mojo_tab));
  return node;
}

}  // namespace tabs_api
