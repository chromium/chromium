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

TabData::TabData(TabID id,
                 TabStripModel* model,
                 GURL url,
                 absl::optional<int> index)
    : tab_id_(id),
      original_tab_strip_model_(model),
      original_url_(url),
      original_index_(index) {}

TabData::TabData(TabStripModel* model, content::WebContents* web_contents) {
  CHECK(model);
  CHECK(web_contents);

  this->tab_id_ = kNextTabID;
  kNextTabID++;

  this->original_tab_strip_model_ = model;
  this->original_index_ = model->GetIndexOfWebContents(web_contents);
  this->original_url_ = web_contents->GetLastCommittedURL();
}
