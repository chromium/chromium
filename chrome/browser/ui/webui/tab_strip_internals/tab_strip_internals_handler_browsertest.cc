// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip_internals/tab_strip_internals_handler.h"

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {

// Minimal Renderer-side (JS) page to receive updates from PageHandler.
class TestPage : public tab_strip_internals::mojom::Page {
 public:
  void OnTabStripUpdated(tab_strip_internals::mojom::ContainerPtr) override {}
};

size_t CountTabs(const tab_strip_internals::mojom::Node* node) {
  size_t count = 0;
  if (!node) {
    return 0;
  }

  if (node->data->which() == tab_strip_internals::mojom::Data::Tag::kTab) {
    return 1;
  }

  for (const auto& child : node->children) {
    count += CountTabs(child.get());
  }
  return count;
}

const tab_strip_internals::mojom::WindowNode* FindWindowBySessionId(
    const tab_strip_internals::mojom::ContainerPtr& data,
    const std::size_t& session_id) {
  for (const auto& window : data->tabstrip_tree->windows) {
    if (window->id->node_id == base::NumberToString(session_id)) {
      return window.get();
    }
  }
  return nullptr;
}

const tab_strip_internals::mojom::Node* FindTabGroupCollection(
    const tab_strip_internals::mojom::Node* root) {
  if (!root) {
    return nullptr;
  }

  for (const auto& child : root->children) {
    if (child->data->which() ==
        tab_strip_internals::mojom::Data::Tag::kUnpinnedTabCollection) {
      for (const auto& unpinned_child : child->children) {
        if (unpinned_child->data->which() ==
            tab_strip_internals::mojom::Data::Tag::kTabGroupCollection) {
          return unpinned_child.get();
        }
      }
    }
  }
  return nullptr;
}

void AssertEmptyRestoreState(
    const tab_strip_internals::mojom::ContainerPtr& data) {
  ASSERT_TRUE(data);
  ASSERT_TRUE(data->tabstrip_tree);
  ASSERT_TRUE(data->tab_restore);
  ASSERT_TRUE(data->restored_session);
  ASSERT_TRUE(data->saved_session);

  EXPECT_TRUE(data->tab_restore->entries.empty());
  EXPECT_TRUE(data->restored_session->entries.empty());
  EXPECT_TRUE(data->saved_session->entries.empty());
}

void AssertValidSelectionModel(
    const tab_strip_internals::mojom::WindowNode* window) {
  ASSERT_TRUE(window);
  ASSERT_TRUE(window->tabstrip_model);
  ASSERT_TRUE(window->selection_model);

  const auto* root = window->tabstrip_model->root.get();
  const size_t tab_count = CountTabs(root);

  const auto& selection = window->selection_model;

  if (tab_count == 0) {
    EXPECT_EQ(selection->active_index, -1);
    EXPECT_EQ(selection->anchor_index, -1);
    EXPECT_TRUE(selection->selected_indices.empty());
    return;
  }

  EXPECT_GE(selection->active_index, 0);
  EXPECT_LT(selection->active_index, static_cast<int>(tab_count));
  EXPECT_GE(selection->anchor_index, 0);
  EXPECT_LT(selection->anchor_index, static_cast<int>(tab_count));
  EXPECT_FALSE(selection->selected_indices.empty());
  EXPECT_TRUE(std::ranges::contains(selection->selected_indices,
                                    selection->active_index));

  for (int index : selection->selected_indices) {
    EXPECT_GE(index, 0);
    EXPECT_LT(index, static_cast<int>(tab_count));
  }
}

}  // namespace

// Tests that TabStripInternalsPageHandler returns the required TabStrip data.
class TabStripInternalsPageHandlerBrowserTest : public InProcessBrowserTest {
 protected:
  std::unique_ptr<TabStripInternalsPageHandler> CreateHandler(
      content::WebContents* web_contents) {
    mojo::PendingRemote<tab_strip_internals::mojom::Page> page_remote;
    mojo::Receiver<tab_strip_internals::mojom::Page> page_receiver_ =
        mojo::Receiver<tab_strip_internals::mojom::Page>(
            &page_impl_, page_remote.InitWithNewPipeAndPassReceiver());

    mojo::PendingRemote<tab_strip_internals::mojom::PageHandler> handler_remote;
    mojo::PendingReceiver<tab_strip_internals::mojom::PageHandler>
        handler_receiver = handler_remote.InitWithNewPipeAndPassReceiver();

    return std::make_unique<TabStripInternalsPageHandler>(
        web_contents, std::move(handler_receiver), std::move(page_remote));
  }

  BrowserWindowInterface* QuitBrowserAndRestore(
      BrowserWindowInterface* browser) {
    Profile* const profile = browser->GetProfile();
    // Session restore pref must be set to LAST to test browser close and
    // restore.
    SessionStartupPref pref(SessionStartupPref::LAST);
    SessionStartupPref::SetStartupPref(profile, pref);
    // Keep the browser and profile alive while closing the browser to trigger
    // session restore.
    ScopedKeepAlive keep_alive(KeepAliveOrigin::SESSION_RESTORE,
                               KeepAliveRestartOption::DISABLED);
    ScopedProfileKeepAlive profile_keep_alive(
        profile, ProfileKeepAliveOrigin::kBrowserWindow);
    CloseBrowserSynchronously(browser);

    // Create a new window, which will trigger session restore.
    ui_test_utils::OpenNewEmptyWindowAndWaitUntilActivated(profile, true);
    return GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  }

 private:
  TestPage page_impl_;
};

// GetTabStripData: Verify snapshot for window with single tab.
IN_PROC_BROWSER_TEST_F(TabStripInternalsPageHandlerBrowserTest,
                       GetTabStripData_EmptyWindow) {
  auto handler =
      CreateHandler(browser()->GetTabStripModel()->GetActiveWebContents());

  base::RunLoop loop;
  handler->GetTabStripData(base::BindOnce(
      [](base::RunLoop* loop, tab_strip_internals::mojom::ContainerPtr data) {
        AssertEmptyRestoreState(data);

        ASSERT_EQ(data->tabstrip_tree->windows.size(), 1u);
        const auto& window = data->tabstrip_tree->windows[0];
        AssertValidSelectionModel(window.get());
        EXPECT_EQ(CountTabs(window->tabstrip_model->root.get()), 1u);

        loop->Quit();
      },
      &loop));

  loop.Run();
}

// GetTabStripData: Verify snapshot for window with multiple tabs.
IN_PROC_BROWSER_TEST_F(TabStripInternalsPageHandlerBrowserTest,
                       GetTabStripData_SingleWindowMultipleTabs) {
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(2, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  auto handler =
      CreateHandler(browser()->GetTabStripModel()->GetActiveWebContents());

  base::RunLoop loop;
  handler->GetTabStripData(base::BindOnce(
      [](base::RunLoop* loop, tab_strip_internals::mojom::ContainerPtr data) {
        AssertEmptyRestoreState(data);

        ASSERT_EQ(data->tabstrip_tree->windows.size(), 1u);
        const auto& window = data->tabstrip_tree->windows[0];
        AssertValidSelectionModel(window.get());
        EXPECT_EQ(CountTabs(window->tabstrip_model->root.get()), 3u);

        loop->Quit();
      },
      &loop));

  loop.Run();
}

// GetTabStripData: Verify snapshot for multiple windows with multiple tabs.
IN_PROC_BROWSER_TEST_F(TabStripInternalsPageHandlerBrowserTest,
                       GetTabStripData_MultipleWindowsAndTabs) {
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  Browser* second_browser = CreateBrowser(browser()->profile());
  ASSERT_TRUE(second_browser);
  ASSERT_TRUE(AddTabAtIndexToBrowser(
      second_browser, 1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  const size_t first_session_id = browser()->GetSessionID().id();
  const size_t second_session_id = second_browser->GetSessionID().id();

  auto handler =
      CreateHandler(browser()->GetTabStripModel()->GetActiveWebContents());

  base::RunLoop loop;
  handler->GetTabStripData(base::BindOnce(
      [](base::RunLoop* loop, size_t first_session_id, size_t second_session_id,
         tab_strip_internals::mojom::ContainerPtr data) {
        AssertEmptyRestoreState(data);
        ASSERT_EQ(data->tabstrip_tree->windows.size(), 2u);

        const auto& win1 = data->tabstrip_tree->windows[0];
        const auto& win2 = data->tabstrip_tree->windows[1];
        AssertValidSelectionModel(win1.get());
        AssertValidSelectionModel(win2.get());
        EXPECT_EQ(CountTabs(win1->tabstrip_model->root.get()), 2u);
        EXPECT_EQ(CountTabs(win2->tabstrip_model->root.get()), 2u);

        loop->Quit();
      },
      &loop, first_session_id, second_session_id));

  loop.Run();
  CloseBrowserSynchronously(second_browser);
}

// GetTabStripData: Verify snapshot includes OTR window.
IN_PROC_BROWSER_TEST_F(TabStripInternalsPageHandlerBrowserTest,
                       GetTabStripData_IncludesOTRWindow) {
  Browser* otr_browser = CreateIncognitoBrowser(browser()->profile());
  ASSERT_TRUE(otr_browser);

  ASSERT_TRUE(AddTabAtIndexToBrowser(otr_browser, 1, GURL(url::kAboutBlankURL),
                                     ui::PAGE_TRANSITION_TYPED));

  const size_t regular_id = browser()->GetSessionID().id();
  const size_t otr_id = otr_browser->GetSessionID().id();

  auto handler =
      CreateHandler(browser()->GetTabStripModel()->GetActiveWebContents());

  base::RunLoop loop;
  handler->GetTabStripData(base::BindOnce(
      [](base::RunLoop* loop, size_t regular_id, size_t otr_id,
         tab_strip_internals::mojom::ContainerPtr data) {
        AssertEmptyRestoreState(data);

        ASSERT_EQ(data->tabstrip_tree->windows.size(), 2u);

        const auto* regular_browser = FindWindowBySessionId(data, regular_id);
        const auto* otr_browser = FindWindowBySessionId(data, otr_id);

        AssertValidSelectionModel(regular_browser);
        AssertValidSelectionModel(otr_browser);
        EXPECT_EQ(CountTabs(regular_browser->tabstrip_model->root.get()), 1u);
        EXPECT_EQ(CountTabs(otr_browser->tabstrip_model->root.get()), 2u);

        loop->Quit();
      },
      &loop, regular_id, otr_id));

  loop.Run();
  CloseBrowserSynchronously(otr_browser);
}

// GetTabStripData: Verify snapshot includes TabRestoreEntry.
IN_PROC_BROWSER_TEST_F(TabStripInternalsPageHandlerBrowserTest,
                       GetTabStripData_TabRestoreAfterTabClose) {
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  // Close tab to add a TabRestore entry.
  chrome::CloseTab(browser());

  auto handler =
      CreateHandler(browser()->GetTabStripModel()->GetActiveWebContents());

  base::RunLoop loop;
  handler->GetTabStripData(base::BindOnce(
      [](base::RunLoop* loop, tab_strip_internals::mojom::ContainerPtr data) {
        ASSERT_TRUE(data);
        ASSERT_TRUE(data->tab_restore);

        EXPECT_FALSE(data->tab_restore->entries.empty())
            << "Expected TabRestore entries after tab close";
        EXPECT_TRUE(data->restored_session->entries.empty());
        EXPECT_TRUE(data->saved_session->entries.empty());

        const auto& entries = data->tab_restore->entries;
        ASSERT_EQ(entries.size(), 1u)
            << "Expected exactly one TabRestore entry after closing a tab";
        const auto& entry = entries[0];
        ASSERT_TRUE(entry->is_tab())
            << "Expected a TabRestore TAB entry after closing a single tab";

        loop->Quit();
      },
      &loop));

  loop.Run();
}

// GetTabStripData: Verify snapshot includes SavedSession data after restart.
IN_PROC_BROWSER_TEST_F(
    TabStripInternalsPageHandlerBrowserTest,
    GetTabStripData_SessionRestore_SavedSession_AfterRestart) {
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(2, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  // Close browser and restore session.
  auto* restored_browser = QuitBrowserAndRestore(browser());
  ASSERT_TRUE(restored_browser);

  // Create PageHandler to capture SavedSession data in the GetTabStripData
  // response.
  // Note: `saved_session` data is persisted on disk and will be
  // available regardless of whether the PageHandler is created before or after
  // browser restart.
  auto handler = CreateHandler(
      restored_browser->GetTabStripModel()->GetActiveWebContents());

  base::RunLoop loop;
  handler->GetTabStripData(base::BindOnce(
      [](base::RunLoop* loop, tab_strip_internals::mojom::ContainerPtr data) {
        ASSERT_TRUE(data);
        ASSERT_TRUE(data->saved_session);

        EXPECT_FALSE(data->saved_session->entries.empty())
            << "Expected saved_session entries after browser restart";
        const auto& saved_entries = data->saved_session->entries;
        ASSERT_GE(saved_entries.size(), 1u);

        const auto& saved_session_window = saved_entries[0];
        EXPECT_GE(saved_session_window->tabs.size(), 1u);
        EXPECT_GE(saved_session_window->selected_tab_index, 0);

        loop->Quit();
      },
      &loop));

  loop.Run();
}

// GetTabStripData: Verify snapshot includes RestoredSession data after restart.
IN_PROC_BROWSER_TEST_F(TabStripInternalsPageHandlerBrowserTest,
                       GetTabStripData_SessionRestoreAfterRestart) {
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(2, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  // Create the PageHandler before closing the browser so it can observe
  // session restore events and capture `restored_session` data.
  // Note: `saved_session` data is persisted on disk and will be available
  // regardless of whether the PageHandler is created before or after browser
  // restart.
  auto handler =
      CreateHandler(browser()->GetTabStripModel()->GetActiveWebContents());

  // Close browser and restore session.
  auto* restored_browser = QuitBrowserAndRestore(browser());
  ASSERT_TRUE(restored_browser);

  base::RunLoop loop;
  handler->GetTabStripData(base::BindOnce(
      [](base::RunLoop* loop, tab_strip_internals::mojom::ContainerPtr data) {
        ASSERT_TRUE(data);
        ASSERT_TRUE(data->tabstrip_tree);
        ASSERT_TRUE(data->restored_session);
        ASSERT_TRUE(data->saved_session);

        EXPECT_FALSE(data->saved_session->entries.empty())
            << "Expected saved_session entries after browser restart";
        const auto& saved_entries = data->saved_session->entries;
        ASSERT_GE(saved_entries.size(), 1u);

        const auto& saved_session_window = saved_entries[0];
        EXPECT_GE(saved_session_window->tabs.size(), 1u);
        EXPECT_GE(saved_session_window->selected_tab_index, 0);

        EXPECT_FALSE(data->restored_session->entries.empty())
            << "Expected restored_session entries after browser restart";
        const auto& restored_entries = data->restored_session->entries;
        ASSERT_GE(restored_entries.size(), 1u);

        const auto& restored_session_window = restored_entries[0];
        EXPECT_GE(restored_session_window->tabs.size(), 1u);
        EXPECT_GE(restored_session_window->selected_tab_index, 0);

        loop->Quit();
      },
      &loop));

  loop.Run();
}

// GetTabStripData: Verify snapshot includes TabGroups in TabStripTree.
IN_PROC_BROWSER_TEST_F(TabStripInternalsPageHandlerBrowserTest,
                       GetTabStripData_TabGroupAppearsInTree) {
  ASSERT_TRUE(browser()->GetTabStripModel()->SupportsTabGroups());
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(2, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  TabStripModel* model = browser()->GetTabStripModel();
  tab_groups::TabGroupId group_id = model->AddToNewGroup({1, 2});
  ASSERT_TRUE(model->group_model()->ContainsTabGroup(group_id));

  auto handler =
      CreateHandler(browser()->GetTabStripModel()->GetActiveWebContents());

  base::RunLoop loop;
  handler->GetTabStripData(base::BindOnce(
      [](base::RunLoop* loop, tab_strip_internals::mojom::ContainerPtr data) {
        AssertEmptyRestoreState(data);

        ASSERT_EQ(data->tabstrip_tree->windows.size(), 1u);
        const auto& window = data->tabstrip_tree->windows[0];
        AssertValidSelectionModel(window.get());

        const auto* root = window->tabstrip_model->root.get();
        ASSERT_TRUE(root);

        const auto* group_collection = FindTabGroupCollection(root);
        ASSERT_TRUE(group_collection)
            << "Expected TabGroupCollection node in tab strip tree";

        // Verify two tabs exist in the group collection.
        EXPECT_EQ(group_collection->children.size(), 2u);

        loop->Quit();
      },
      &loop));

  loop.Run();
}

// GetTabStripData: Verify snapshot includes multiple TabGroups in TabStripTree.
IN_PROC_BROWSER_TEST_F(TabStripInternalsPageHandlerBrowserTest,
                       GetTabStripData_MultipleTabGroups) {
  ASSERT_TRUE(browser()->GetTabStripModel()->SupportsTabGroups());

  for (int i = 1; i <= 4; ++i) {
    ASSERT_TRUE(
        AddTabAtIndex(i, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  }

  TabStripModel* model = browser()->GetTabStripModel();
  tab_groups::TabGroupId group1 = model->AddToNewGroup({0, 1});
  tab_groups::TabGroupId group2 = model->AddToNewGroup({2, 3});

  ASSERT_TRUE(model->group_model()->ContainsTabGroup(group1));
  ASSERT_TRUE(model->group_model()->ContainsTabGroup(group2));

  auto handler =
      CreateHandler(browser()->GetTabStripModel()->GetActiveWebContents());

  base::RunLoop loop;
  handler->GetTabStripData(base::BindOnce(
      [](base::RunLoop* loop, tab_strip_internals::mojom::ContainerPtr data) {
        AssertEmptyRestoreState(data);

        const auto& window = data->tabstrip_tree->windows[0];
        const auto* root = window->tabstrip_model->root.get();
        ASSERT_TRUE(root);

        const auto* group_collection = FindTabGroupCollection(root);
        ASSERT_TRUE(group_collection);

        // Verify two tabs exist in one of the group collections.
        EXPECT_EQ(group_collection->children.size(), 2u);

        // Verify total tab count unchanged
        EXPECT_EQ(CountTabs(root), 5u);

        loop->Quit();
      },
      &loop));

  loop.Run();
}

// GetTabStripData: Verify snapshot includes TabRestoreGroup entry.
IN_PROC_BROWSER_TEST_F(TabStripInternalsPageHandlerBrowserTest,
                       GetTabStripData_TabRestore_GroupEntryCreated) {
  ASSERT_TRUE(browser()->GetTabStripModel()->SupportsTabGroups());
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(2, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  TabStripModel* model = browser()->GetTabStripModel();
  tab_groups::TabGroupId group_id = model->AddToNewGroup({1, 2});
  ASSERT_TRUE(model->group_model()->ContainsTabGroup(group_id));

  // Push entries into TabRestoreService.
  model->CloseAllTabsInGroup(group_id);

  Browser* new_browser = CreateBrowser(browser()->profile());
  ASSERT_TRUE(new_browser);
  auto handler =
      CreateHandler(new_browser->GetTabStripModel()->GetActiveWebContents());

  base::RunLoop loop;
  handler->GetTabStripData(base::BindOnce(
      [](base::RunLoop* loop, tab_strip_internals::mojom::ContainerPtr data) {
        ASSERT_TRUE(data);
        ASSERT_TRUE(data->tab_restore);

        size_t group_count = 0;
        for (const auto& entry : data->tab_restore->entries) {
          if (entry->is_group()) {
            group_count++;
          }
        }

        EXPECT_EQ(group_count, 1u)
            << "Expected exactly one TabRestoreGroup entry";

        loop->Quit();
      },
      &loop));

  loop.Run();
}

// GetTabStripData: Verify snapshot includes TabGroups in SessionRestore data.
IN_PROC_BROWSER_TEST_F(TabStripInternalsPageHandlerBrowserTest,
                       GetTabStripData_SessionRestore_TabGroupsPreserved) {
  ASSERT_TRUE(browser()->GetTabStripModel()->SupportsTabGroups());
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(2, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  TabStripModel* model = browser()->GetTabStripModel();
  tab_groups::TabGroupId group_id = model->AddToNewGroup({1, 2});
  ASSERT_TRUE(model->group_model()->ContainsTabGroup(group_id));

  auto handler =
      CreateHandler(browser()->GetTabStripModel()->GetActiveWebContents());
  // Trigger restore.
  auto* restored_browser = QuitBrowserAndRestore(browser());
  ASSERT_TRUE(restored_browser);

  base::RunLoop loop;
  handler->GetTabStripData(base::BindOnce(
      [](base::RunLoop* loop, tab_strip_internals::mojom::ContainerPtr data) {
        ASSERT_TRUE(data);

        ASSERT_TRUE(data->restored_session);
        ASSERT_EQ(data->restored_session->entries.size(), 1u);
        const auto& restored_session_window =
            data->restored_session->entries[0];
        EXPECT_EQ(restored_session_window->tab_groups.size(), 1u)
            << "Expected exactly one restored session tab group";

        ASSERT_TRUE(data->saved_session);
        ASSERT_EQ(data->saved_session->entries.size(), 1u);
        const auto& saved_session_window = data->saved_session->entries[0];
        EXPECT_EQ(saved_session_window->tab_groups.size(), 1u)
            << "Expected exactly one saved session tab group";

        loop->Quit();
      },
      &loop));

  loop.Run();
}
