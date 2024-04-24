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
      owning_model_(owning_model),
      is_in_normal_window_(owning_model->delegate()->IsNormalWindow()) {
  // When a TabModel is constructed it must be attached to a TabStripModel. This
  // may later change if the Tab is detached.
  CHECK(owning_model);
  owning_model_->AddObserver(this);

  tab_features_ = TabFeatures::CreateTabFeatures();

  // Once tabs are pulled into a standalone module, TabFeatures and its
  // initialization will need to be delegated back to the main module.
  tab_features_->Init(this, owning_model_->profile());
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

void TabModel::WillEnterBackground(base::PassKey<TabStripModel>) {
  will_enter_background_callback_list_.Notify(this);
}

content::WebContents* TabModel::GetContents() const {
  return contents();
}

base::CallbackListSubscription TabModel::RegisterWillDiscardContents(
    TabInterface::WillDiscardContentsCallback callback) {
  return will_discard_contents_callback_list_.Add(std::move(callback));
}

bool TabModel::IsInForeground() const {
  return owning_model() && owning_model()->GetActiveTab() == this;
}

base::CallbackListSubscription TabModel::RegisterDidEnterForeground(
    TabInterface::DidEnterForegroundCallback callback) {
  return did_enter_foreground_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription TabModel::RegisterWillEnterBackground(
    TabInterface::WillEnterBackgroundCallback callback) {
  return will_enter_background_callback_list_.Add(std::move(callback));
}

bool TabModel::CanShowModalUI() const {
  return !showing_modal_ui_;
}

std::unique_ptr<ScopedTabModalUI> TabModel::ShowModalUI() {
  return std::make_unique<ScopedTabModalUIImpl>(this);
}

bool TabModel::IsInNormalWindow() const {
  return is_in_normal_window_;
}

BrowserWindowInterface* TabModel::GetBrowserWindowInterface() {
  return owning_model_->delegate()->GetBrowserWindowInterface();
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

std::unique_ptr<content::WebContents> TabModel::DiscardContents(
    std::unique_ptr<content::WebContents> contents) {
  will_discard_contents_callback_list_.Notify(this, contents_, contents.get());
  std::unique_ptr<content::WebContents> old_contents =
      std::move(contents_owned_);
  contents_owned_ = std::move(contents);
  contents_ = contents_owned_.get();
  return old_contents;
}

// static
std::unique_ptr<content::WebContents> TabModel::DestroyAndTakeWebContents(
    std::unique_ptr<TabModel> tab_model) {
  std::unique_ptr<content::WebContents> contents =
      std::move(tab_model->contents_owned_);
  return contents;
}

}  // namespace tabs
