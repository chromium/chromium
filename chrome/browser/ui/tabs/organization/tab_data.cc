// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_data.h"

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace {
// TODO(1476012) replace with opaque tab handle
int kNextTabID = 1;
}  // namespace

TabData::TabData(TabStripModel* model, content::WebContents* web_contents)
    : tab_id_(kNextTabID),
      web_contents_(web_contents),
      original_url_(web_contents->GetLastCommittedURL()) {
  CHECK(model);
  CHECK(web_contents);

  kNextTabID++;

  original_tab_strip_model_ = model;
  model->AddObserver(this);
}

TabData::~TabData() {
  if (original_tab_strip_model_) {
    original_tab_strip_model_->RemoveObserver(this);
  }
}

bool TabData::IsValidForOrganizing() const {
  // if the model or the web_contents have been destroyed, then it's not valid.
  if (!original_tab_strip_model_ || !web_contents_) {
    return false;
  }

  // If the web_contents is no longer the same URL, then it's not valid.
  if (original_url_ != web_contents_->GetLastCommittedURL()) {
    return false;
  }

  if (original_tab_strip_model_
          ->GetTabGroupForTab(
              original_tab_strip_model_->GetIndexOfWebContents(web_contents_))
          .has_value()) {
    return false;
  }

  return true;
}

void TabData::OnTabStripModelDestroyed(TabStripModel* tab_strip_model) {
  if (original_tab_strip_model_ == tab_strip_model) {
    original_tab_strip_model_ = nullptr;
    web_contents_ = nullptr;
  }
}

void TabData::OnTabStripModelChanged(TabStripModel* tab_strip_model,
                                     const TabStripModelChange& change,
                                     const TabStripSelectionChange& selection) {
  // If the webcontents has already been destroyed, then further updates dont
  // matter.
  if (web_contents_ == nullptr) {
    return;
  }

  switch (change.type()) {
    // For replaced webcontents, just replace the underlying webcontents.
    case TabStripModelChange::Type::kReplaced: {
      const TabStripModelChange::Replace* replace = change.GetReplace();
      if (replace->old_contents == web_contents_) {
        web_contents_ = replace->new_contents;
      }
      return;
    }
    // If the tab is removed, then delete the webcontents, and mark as invalid.
    case TabStripModelChange::Type::kRemoved: {
      const TabStripModelChange::Remove* remove = change.GetRemove();
      for (const TabStripModelChange::RemovedTab& removed_tab :
           remove->contents) {
        if (removed_tab.contents == web_contents_) {
          web_contents_ = nullptr;
        }
      }
      return;
    }
    default: {
      return;
    }
  }
}
