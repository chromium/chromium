// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_jni_bridge.h"

#include <optional>

#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/range/range.h"

namespace {

using TabModelJniBridgeTest = AndroidBrowserTest;

IN_PROC_BROWSER_TEST_F(TabModelJniBridgeTest, AddToGroupWithOutOfOrderHandles) {
  // Create some tabs. This is likely, though not guaranteed, to create
  // TabHandles with increasing internal values.
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);
  tabs::TabInterface* tab_a = tab_list->GetTab(0);
  tabs::TabInterface* tab_b =
      tab_list->OpenTab(GURL("about:blank"), /*index=*/-1);
  tabs::TabInterface* tab_c =
      tab_list->OpenTab(GURL("about:blank"), /*index=*/-1);

  // Move the last tab to the front, so its order in the tab strip doesn't
  // match the internal TabHandle order.
  tab_list->MoveTab(tab_c->GetHandle(), 0);

  // The tab strip order changed to C, A, B.
  ASSERT_EQ(tab_c->GetHandle(), tab_list->GetTab(0)->GetHandle());
  ASSERT_EQ(tab_a->GetHandle(), tab_list->GetTab(1)->GetHandle());
  ASSERT_EQ(tab_b->GetHandle(), tab_list->GetTab(2)->GetHandle());

  // Create a group from the tabs. Internally the set will likely be ordered
  // A, B, C based on the creation order of the handles (though this is not
  // guaranteed).
  std::set<tabs::TabHandle> handles{tab_a->GetHandle(), tab_b->GetHandle(),
                                    tab_c->GetHandle()};
  tab_list->AddTabsToGroup(std::nullopt, handles);

  // The tab strip order is still C, A, B (not the order of the handles).
  EXPECT_EQ(tab_c->GetHandle(), tab_list->GetTab(0)->GetHandle());
  EXPECT_EQ(tab_a->GetHandle(), tab_list->GetTab(1)->GetHandle());
  EXPECT_EQ(tab_b->GetHandle(), tab_list->GetTab(2)->GetHandle());
}

IN_PROC_BROWSER_TEST_F(TabModelJniBridgeTest, InsertWebContentsAt) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);
  int initial_count = tab_list->GetTabCount();

  // Insert WebContents to a new tab at index 1.
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfile()));
  auto* web_contents_ptr = web_contents.get();
  tabs::TabInterface* new_tab = tab_list->InsertWebContentsAt(
      /*index=*/1, std::move(web_contents), /*should_pin=*/false,
      /*group=*/std::nullopt);

  // Check the new tab.
  ASSERT_TRUE(new_tab);
  EXPECT_EQ(initial_count + 1, tab_list->GetTabCount());
  EXPECT_EQ(new_tab, tab_list->GetTab(1));
  EXPECT_EQ(web_contents_ptr, new_tab->GetContents());
}

}  // namespace
