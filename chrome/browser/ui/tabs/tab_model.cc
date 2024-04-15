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
    : contents_owned_(std::move(contents)),
      contents_(contents_owned_.get()),
      owning_model_(owning_model) {
  // When a TabModel is constructed it must be attached to a TabStripModel. This
  // may later change if the Tab is detached.
  CHECK(owning_model);
  owning_model_->AddObserver(this);

  tab_features_ = TabFeatures::CreateTabFeatures();
  tab_features_->Init(this);
}

TabModel::~TabModel() = default;

void TabModel::OnAddedToModel(TabStripModel* owning_model) {
  CHECK(!owning_model_);
  CHECK(owning_model);
  owning_model_ = owning_model;
  owning_model_->AddObserver(this);

  // Being detached is equivalent to being in the background. So after
  // detachment, if the tab is in the foreground, we must send a notification.
  if (IsInForeground()) {
    did_enter_foreground_callback_list_.Notify(this);
  }
}

void TabModel::OnRemovedFromModel() {
  // Going through each field here:
  // Keep `contents_`, obviously.

  // We are now unowned. In this case no UI is shown, which is functionally
  // equivalent to being in the background.
  did_enter_background_callback_list_.Notify(this);
  owning_model_->RemoveObserver(this);
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

content::WebContents* TabModel::GetContents() const {
  return contents();
}

base::CallbackListSubscription TabModel::RegisterDidAddContents(
    TabInterface::DidAddContentsCallback callback) {
  return did_add_contents_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription TabModel::RegisterWillRemoveContents(
    TabInterface::WillRemoveContentsCallback callback) {
  return will_remove_contents_callback_list_.Add(std::move(callback));
}

bool TabModel::IsInForeground() const {
  return owning_model()->GetActiveTab() == this;
}

base::CallbackListSubscription TabModel::RegisterDidEnterForeground(
    TabInterface::DidEnterForegroundCallback callback) {
  return did_enter_foreground_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription TabModel::RegisterDidEnterBackground(
    TabInterface::DidEnterBackgroundCallback callback) {
  return did_enter_background_callback_list_.Add(std::move(callback));
}

bool TabModel::CanShowModalUI() const {
  return !showing_modal_ui_;
}

std::unique_ptr<ScopedTabModalUI> TabModel::ShowModalUI() {
  return std::make_unique<ScopedTabModalUIImpl>(this);
}

void TabModel::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!selection.active_tab_changed()) {
    return;
  }

  if (selection.new_contents == contents()) {
    did_enter_foreground_callback_list_.Notify(this);
    return;
  }

  if (selection.old_contents == contents()) {
    did_enter_background_callback_list_.Notify(this);
  }
}

TabModel::ScopedTabModalUIImpl::ScopedTabModalUIImpl(TabModel* tab)
    : tab_(tab) {
  CHECK(!tab_->showing_modal_ui_);
  tab_->showing_modal_ui_ = true;
}

TabModel::ScopedTabModalUIImpl::~ScopedTabModalUIImpl() {
  tab_->showing_modal_ui_ = false;
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
  will_remove_contents_callback_list_.Notify(this, contents_.get());
  contents_ = nullptr;
  return std::move(contents_owned_);
}

// static
std::unique_ptr<content::WebContents> TabModel::DestroyAndTakeWebContents(
    std::unique_ptr<TabModel> tab_model) {
  std::unique_ptr<content::WebContents> contents =
      std::move(tab_model->contents_owned_);
  return contents;
}

void TabModel::SetContents(std::unique_ptr<content::WebContents> contents) {
  CHECK(!contents_);
  CHECK(contents);
  contents_owned_ = std::move(contents);
  contents_ = contents_owned_.get();
  for (auto& obs : observers_) {
    obs.DidAddContents(this, contents_.get());
  }
  did_add_contents_callback_list_.Notify(this, contents_.get());
}

}  // namespace tabs
