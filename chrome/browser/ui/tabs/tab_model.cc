// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_model.h"

#include "chrome/browser/ui/tabs/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"

namespace tabs {

TabModel::TabModel(std::unique_ptr<content::WebContents> contents,
                   TabStripModel* owning_model)
    : contents_(std::move(contents)), owning_model_(owning_model) {
  // When a TabModel is constructed it must be attached to a TabStripModel. This
  // may later change if the Tab is detached.
  CHECK(owning_model);

  tab_features_ = TabFeatures::CreateTabFeatures();
  tab_features_->Init(this);
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

std::unique_ptr<content::WebContents> TabModel::ReplaceContents(
    std::unique_ptr<content::WebContents> contents) {
  std::unique_ptr<content::WebContents> old_contents = RemoveContents();
  SetContents(std::move(contents));
  return old_contents;
}

std::unique_ptr<content::WebContents> TabModel::RemoveContents() {
  for (auto& obs : observers_) {
    obs.WillRemoveContents(this, contents_.get());
  }
  return std::move(contents_);
}

void TabModel::SetContents(std::unique_ptr<content::WebContents> contents) {
  CHECK(!contents_);
  CHECK(contents);
  contents_ = std::move(contents);
  for (auto& obs : observers_) {
    obs.DidAddContents(this, contents_.get());
  }
}

}  // namespace tabs
