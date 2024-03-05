// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_model.h"

#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"

namespace tabs {

TabModel::TabModel(std::unique_ptr<content::WebContents> contents,
                   TabStripModel* owning_model)
    : contents_(std::move(contents)), owning_model_(owning_model) {
  CHECK(owning_model);
  lens_overlay_controller_ = std::make_unique<LensOverlayController>(this);
}

TabModel::~TabModel() = default;

void TabModel::OnAddedToModel(TabStripModel* owning_model) {
  CHECK(owning_model);
  owning_model_ = owning_model;
}

void TabModel::OnRemovedFromModel() {
  // Going through each field here:
  // Keep `contents_`, obviously.

  // We are now unowned.
  owning_model_ = nullptr;

  // Opener stuff doesn't make sense to transfer between browsers.
  opener_ = nullptr;
  reset_opener_on_active_tab_change_ = false;

  // Pinned state, blocked state, and group membership are all preserved, at
  // least in some cases, but for now let's leave that to the existing
  // mechanisms that were handling that.
  // TODO(tbergquist): Decide whether to stick with this approach or not.
  pinned_ = false;
  blocked_ = false;
  group_ = std::nullopt;
}

TabCollection* TabModel::GetParentCollection(
    base::PassKey<TabCollection>) const {
  CHECK(base::FeatureList::IsEnabled(features::kTabStripCollectionStorage));
  return parent_collection_;
}

void TabModel::OnReparented(TabCollection* parent,
                            base::PassKey<TabCollection>) {
  CHECK(base::FeatureList::IsEnabled(features::kTabStripCollectionStorage));
  parent_collection_ = parent;
}

void TabModel::WriteIntoTrace(perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("web_contents", contents());
  dict.Add("pinned", pinned());
  dict.Add("blocked", blocked());
}

}  // namespace tabs
