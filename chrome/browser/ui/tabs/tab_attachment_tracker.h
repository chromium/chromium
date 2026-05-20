// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_ATTACHMENT_TRACKER_H_
#define CHROME_BROWSER_UI_TABS_TAB_ATTACHMENT_TRACKER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace tabs {

class TabInterface;

// Tracks tab attachment/insertion occurrences. Specifically, it counts how many
// times a tab has been attached (inserted) into a tab strip model (tablist).
class TabAttachmentTracker {
 public:
  DECLARE_USER_DATA(TabAttachmentTracker);

  static TabAttachmentTracker* From(tabs::TabInterface* tab);

  explicit TabAttachmentTracker(tabs::TabInterface* tab);
  TabAttachmentTracker(const TabAttachmentTracker&) = delete;
  TabAttachmentTracker& operator=(const TabAttachmentTracker&) = delete;
  ~TabAttachmentTracker();

  int attachment_count() const { return attachment_count_; }

 private:
  void OnDidInsert(tabs::TabInterface* tab);

  raw_ptr<tabs::TabInterface> tab_;
  int attachment_count_ = 0;
  base::CallbackListSubscription did_insert_subscription_;
  ui::ScopedUnownedUserData<TabAttachmentTracker> scoped_data_holder_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_TAB_ATTACHMENT_TRACKER_H_
