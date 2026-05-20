// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_attachment_tracker.h"

#include "components/tabs/public/tab_interface.h"

namespace tabs {

// static
TabAttachmentTracker* TabAttachmentTracker::From(tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

TabAttachmentTracker::TabAttachmentTracker(tabs::TabInterface* tab)
    : tab_(tab), scoped_data_holder_(tab->GetUnownedUserDataHost(), *this) {
  did_insert_subscription_ = tab_->RegisterDidInsert(base::BindRepeating(
      &TabAttachmentTracker::OnDidInsert, base::Unretained(this)));
}

TabAttachmentTracker::~TabAttachmentTracker() = default;

void TabAttachmentTracker::OnDidInsert(tabs::TabInterface* tab) {
  attachment_count_++;
}

DEFINE_USER_DATA(TabAttachmentTracker);

}  // namespace tabs
