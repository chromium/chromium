// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_embedding_context.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"

using WebUIEmbeddingContextTest = InProcessBrowserTest;

namespace webui {

IN_PROC_BROWSER_TEST_F(
    WebUIEmbeddingContextTest,
    InitEmbeddingContext_MovingTabsAcrossWindowsUpdatesContext) {
  // Create a browser with 2 tabs.
  content::WebContents* tab_contents =
      chrome::AddAndReturnTabAt(browser(), GURL(url::kAboutBlankURL), 1, true);
  tabs::TabInterface* tab_interface =
      browser()->tab_strip_model()->GetTabAtIndex(1);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(tab_interface, GetTabInterface(tab_contents));
  EXPECT_EQ(browser(), GetBrowserWindowInterface(tab_contents));

  base::MockCallback<base::RepeatingClosure> tab_changed_callback;
  base::MockCallback<base::RepeatingClosure> browser_changed_callback;
  base::CallbackListSubscription tab_subscription =
      RegisterTabInterfaceChanged(tab_contents, tab_changed_callback.Get());
  base::CallbackListSubscription browser_subscription =
      RegisterBrowserWindowInterfaceChanged(tab_contents,
                                            browser_changed_callback.Get());

  // Move the tab into a new browser window.
  EXPECT_CALL(tab_changed_callback, Run).Times(0);
  EXPECT_CALL(browser_changed_callback, Run).Times(1);
  ui_test_utils::BrowserChangeObserver new_browser_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  chrome::MoveTabsToNewWindow(browser(), {1});
  Browser* new_browser = new_browser_observer.Wait();
  ASSERT_TRUE(new_browser);
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, new_browser->tab_strip_model()->count());
  EXPECT_EQ(tab_interface, GetTabInterface(tab_contents));
  EXPECT_EQ(new_browser, GetBrowserWindowInterface(tab_contents));
  testing::Mock::VerifyAndClearExpectations(&tab_changed_callback);
  testing::Mock::VerifyAndClearExpectations(&browser_changed_callback);

  // Move the tab back into its original browser window. This results in the
  // closing and destruction of the previously created browser. Only one
  // browser-changed notification is expected as the tab is moved to the new
  // browser before the old browser is destroyed.
  EXPECT_CALL(tab_changed_callback, Run).Times(0);
  EXPECT_CALL(browser_changed_callback, Run).Times(1);
  chrome::MoveTabsToExistingWindow(new_browser, browser(), {0});
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(tab_interface, GetTabInterface(tab_contents));
  EXPECT_EQ(browser(), GetBrowserWindowInterface(tab_contents));
  testing::Mock::VerifyAndClearExpectations(&browser_changed_callback);
}

IN_PROC_BROWSER_TEST_F(WebUIEmbeddingContextTest,
                       InitEmbeddingContext_SetCorrectlyOnTabDiscard) {
  // Create a browser with 2 tabs.
  content::WebContents* tab_contents =
      chrome::AddAndReturnTabAt(browser(), GURL(url::kAboutBlankURL), 1, false);
  tabs::TabInterface* tab_interface =
      browser()->tab_strip_model()->GetTabAtIndex(1);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(tab_interface, GetTabInterface(tab_contents));
  EXPECT_EQ(browser(), GetBrowserWindowInterface(tab_contents));

  // Discard the tab.
  EXPECT_NE(browser()->tab_strip_model()->GetActiveTab(), tab_interface);
  auto* lifecycle_unit =
      resource_coordinator::TabLifecycleUnitSource::GetTabLifecycleUnitExternal(
          tab_contents);
  lifecycle_unit->DiscardTab(mojom::LifecycleUnitDiscardReason::URGENT);

  EXPECT_EQ(mojom::LifecycleUnitState::DISCARDED,
            lifecycle_unit->GetTabState());
  tab_contents = browser()->tab_strip_model()->GetTabAtIndex(1)->GetContents();

  // The tab and browser interfaces should remain associated with the tab
  // contents after discard.
  EXPECT_EQ(tab_interface, GetTabInterface(tab_contents));
  EXPECT_EQ(browser(), GetBrowserWindowInterface(tab_contents));
}

IN_PROC_BROWSER_TEST_F(WebUIEmbeddingContextTest,
                       SetTabInterface_TracksEmbedderStateCorrectly) {
  // Create a new WebContents, the tab and browser should start empty.
  std::unique_ptr<content::WebContents> host_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));
  EXPECT_FALSE(GetTabInterface(host_contents.get()));
  EXPECT_FALSE(GetBrowserWindowInterface(host_contents.get()));

  base::MockCallback<base::RepeatingClosure> tab_changed_callback;
  base::MockCallback<base::RepeatingClosure> browser_changed_callback;
  base::CallbackListSubscription tab_subscription = RegisterTabInterfaceChanged(
      host_contents.get(), tab_changed_callback.Get());
  base::CallbackListSubscription browser_subscription =
      RegisterBrowserWindowInterfaceChanged(host_contents.get(),
                                            browser_changed_callback.Get());

  // Set the tab interface, the tracked state should update.
  EXPECT_CALL(tab_changed_callback, Run).Times(1);
  EXPECT_CALL(browser_changed_callback, Run).Times(1);
  tabs::TabInterface* tab_interface =
      browser()->tab_strip_model()->GetActiveTab();
  SetTabInterface(host_contents.get(), tab_interface);
  EXPECT_EQ(tab_interface, GetTabInterface(host_contents.get()));
  EXPECT_EQ(browser(), GetBrowserWindowInterface(host_contents.get()));
  testing::Mock::VerifyAndClearExpectations(&tab_changed_callback);
  testing::Mock::VerifyAndClearExpectations(&browser_changed_callback);

  // Reset the tab interface, this should be reflected in the tracked state.
  EXPECT_CALL(tab_changed_callback, Run).Times(1);
  EXPECT_CALL(browser_changed_callback, Run).Times(1);
  SetTabInterface(host_contents.get(), nullptr);
  EXPECT_FALSE(GetTabInterface(host_contents.get()));
  EXPECT_FALSE(GetBrowserWindowInterface(host_contents.get()));
}

IN_PROC_BROWSER_TEST_F(WebUIEmbeddingContextTest,
                       SetTabInterface_NotifiesBrowserChanges) {
  // Create a new WebContents and set the emebdding tab interface.
  std::unique_ptr<content::WebContents> host_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));
  chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), 1, true);
  tabs::TabInterface* tab_interface =
      browser()->tab_strip_model()->GetTabAtIndex(1);
  SetTabInterface(host_contents.get(), tab_interface);
  EXPECT_EQ(tab_interface, GetTabInterface(host_contents.get()));
  EXPECT_EQ(browser(), GetBrowserWindowInterface(host_contents.get()));

  base::MockCallback<base::RepeatingClosure> tab_changed_callback;
  base::MockCallback<base::RepeatingClosure> browser_changed_callback;
  base::CallbackListSubscription tab_subscription = RegisterTabInterfaceChanged(
      host_contents.get(), tab_changed_callback.Get());
  base::CallbackListSubscription browser_subscription =
      RegisterBrowserWindowInterfaceChanged(host_contents.get(),
                                            browser_changed_callback.Get());

  // Create a new browser and move the tab over, the appropriate browser changed
  // notification should be fired.
  EXPECT_CALL(tab_changed_callback, Run).Times(0);
  EXPECT_CALL(browser_changed_callback, Run).Times(1);

  Browser* dst_browser = CreateBrowser(browser()->profile());
  std::unique_ptr<tabs::TabModel> detached_tab =
      browser()->tab_strip_model()->DetachTabAtForInsertion(1);
  EXPECT_EQ(tab_interface, detached_tab.get());
  dst_browser->tab_strip_model()->InsertDetachedTabAt(
      1, std::move(detached_tab), AddTabTypes::ADD_NONE);

  EXPECT_EQ(tab_interface, GetTabInterface(host_contents.get()));
  EXPECT_EQ(dst_browser, GetBrowserWindowInterface(host_contents.get()));
}

IN_PROC_BROWSER_TEST_F(WebUIEmbeddingContextTest,
                       SetTabInterface_TrackedStateResetOnEmbedderDestruction) {
  // Create a new WebContents and set the emebdding tab interface.
  std::unique_ptr<content::WebContents> host_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));
  chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), 1, true);
  tabs::TabInterface* tab_interface =
      browser()->tab_strip_model()->GetTabAtIndex(1);
  SetTabInterface(host_contents.get(), tab_interface);
  EXPECT_EQ(tab_interface, GetTabInterface(host_contents.get()));
  EXPECT_EQ(browser(), GetBrowserWindowInterface(host_contents.get()));

  base::MockCallback<base::RepeatingClosure> tab_changed_callback;
  base::MockCallback<base::RepeatingClosure> browser_changed_callback;
  base::CallbackListSubscription tab_subscription = RegisterTabInterfaceChanged(
      host_contents.get(), tab_changed_callback.Get());
  base::CallbackListSubscription browser_subscription =
      RegisterBrowserWindowInterfaceChanged(host_contents.get(),
                                            browser_changed_callback.Get());

  // Destroy the embedding tab, this should be reflected in tracked state.
  EXPECT_CALL(tab_changed_callback, Run).Times(1);
  EXPECT_CALL(browser_changed_callback, Run).Times(1);
  browser()->tab_strip_model()->DetachAndDeleteWebContentsAt(1);
  EXPECT_FALSE(GetTabInterface(host_contents.get()));
  EXPECT_FALSE(GetBrowserWindowInterface(host_contents.get()));
}

}  // namespace webui
