// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_collection_tab_model_impl.h"

#include <optional>

#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"

namespace {

using TabCollectionTabModelImplTest = AndroidBrowserTest;

// Regression test for crash in GetTabGroupTitle().
// http://crbug.com/480858259
IN_PROC_BROWSER_TEST_F(TabCollectionTabModelImplTest,
                       GetTabGroupTitleAfterClose) {
  // Create a couple tabs.
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);
  tabs::TabInterface* tab_a = tab_list->GetTab(0);
  tabs::TabInterface* tab_b =
      tab_list->OpenTab(GURL("about:blank"), /*index=*/-1);

  // Create a tab group (A, B).
  std::optional<tab_groups::TabGroupId> group_id =
      tab_list->CreateTabGroup({tab_a->GetHandle(), tab_b->GetHandle()});
  ASSERT_TRUE(group_id);

  // Close all the tabs in the group.
  tab_list->CloseTab(tab_a->GetHandle());
  tab_list->CloseTab(tab_b->GetHandle());

  // The group is gone.
  EXPECT_FALSE(tab_list->ContainsTabGroup(*group_id));

  // Trying to look up the group title (as part of visual data) does not crash.
  std::optional<tab_groups::TabGroupVisualData> visual_data =
      tab_list->GetTabGroupVisualData(*group_id);
  EXPECT_FALSE(visual_data);
}

}  // namespace
