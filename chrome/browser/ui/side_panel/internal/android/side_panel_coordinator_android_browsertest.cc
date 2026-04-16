// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/internal/android/side_panel_coordinator_android.h"

#include <memory>

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/side_panel/android/side_panel_native_view_android.h"
#include "chrome/browser/ui/side_panel/side_panel_content_proxy.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_waiter.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/side_panel/side_panel_ui_provider.h"
#include "chrome/browser/ui/side_panel/side_panel_util.h"
#include "chrome/browser/ui/side_panel/test/android/browser_test_support_jni/SidePanelCoordinatorAndroidBrowserTestSupport_jni.h"
#include "chrome/browser/ui/side_panel/test/android/side_panel_android_browser_test_base.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/window_android.h"
#include "ui/base/base_window.h"

namespace {
class TestSidePanelEntryObserver final : public SidePanelEntryObserver {
 public:
  TestSidePanelEntryObserver() = default;
  ~TestSidePanelEntryObserver() override = default;

  void OnEntryShown(SidePanelEntry* entry) override {
    id_for_last_entry_shown_ = entry->key().id();
  }

  void OnEntryWillHide(SidePanelEntry* entry,
                       SidePanelEntryHideReason reason) override {
    id_for_last_entry_will_hide_ = entry->key().id();
    reason_for_last_entry_will_hide_ = reason;
  }

  void OnEntryHidden(SidePanelEntry* entry) override {
    id_for_last_entry_hidden_ = entry->key().id();
  }

  void OnEntryHiddenWithReason(SidePanelEntry* entry,
                               SidePanelEntryHideReason reason) override {
    id_for_last_entry_hidden_with_reason_ = entry->key().id();
    reason_for_last_entry_hidden_with_reason_ = reason;
  }

  std::optional<SidePanelEntry::Id> id_for_last_entry_shown_;

  std::optional<SidePanelEntry::Id> id_for_last_entry_will_hide_;
  std::optional<SidePanelEntryHideReason> reason_for_last_entry_will_hide_;

  std::optional<SidePanelEntry::Id> id_for_last_entry_hidden_;

  std::optional<SidePanelEntry::Id> id_for_last_entry_hidden_with_reason_;
  std::optional<SidePanelEntryHideReason>
      reason_for_last_entry_hidden_with_reason_;
};

std::unique_ptr<SidePanelEntry> CreateSidePanelEntry(
    SidePanelEntryKey key,
    BrowserWindowInterface* browser) {
  SidePanelEntry::CreateContentCallback create_content_callback =
      base::BindRepeating(
          [](BrowserWindowInterface* browser, SidePanelEntryScope& scope) {
            ui::WindowAndroid* window_android =
                browser->GetWindow()->GetNativeWindow();
            base::android::ScopedJavaLocalRef<jobject> java_view =
                Java_SidePanelCoordinatorAndroidBrowserTestSupport_createTestView(
                    base::android::AttachCurrentThread(),
                    window_android->GetJavaObject());
            return std::make_unique<SidePanelNativeViewAndroid>(
                base::android::ScopedJavaGlobalRef<jobject>(java_view));
          },
          base::Unretained(browser));

  auto default_content_width_callback = base::RepeatingCallback<int()>();

  return std::make_unique<SidePanelEntry>(SidePanelType::kToolbar, key,
                                          create_content_callback,
                                          default_content_width_callback);
}
}  // namespace

class SidePanelCoordinatorAndroidBrowserTest
    : public SidePanelAndroidBrowserTestBase {
 protected:
  using SidePanelAndroidBrowserTestBase::GetActiveTab;
  using SidePanelAndroidBrowserTestBase::GetBrowserWindow;
};

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       TestSidePanelUIProvider) {
  SidePanelUI* side_panel_ui = SidePanelUIProvider::From(GetBrowserWindow());
  EXPECT_NE(nullptr, side_panel_ui);
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       Show_TriggersOnEntryShown) {
  // Arrange:
  BrowserWindowInterface* browser = GetBrowserWindow();

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser);
  TestSidePanelEntryObserver entry_observer;
  entry->AddObserver(&entry_observer);

  auto* registry = SidePanelRegistry::From(browser);
  registry->Register(std::move(entry));

  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  coordinator->SetNoDelaysForTesting(true);

  // Act:
  // SidePanelCoordinatorAndroid::Show(const UniqueKey&,
  // std::optional<SidePanelOpenTrigger>, bool) is protected, so we use
  // SidePanelUIBase to call SidePanelCoordinatorAndroid::Show().
  coordinator->SidePanelUIBase::Show(entry_key, /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);

  // Assert:
  EXPECT_EQ(entry_key.id(), entry_observer.id_for_last_entry_shown_.value());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       Show_TabScopedEntry_SetsActiveEntry) {
  // Arrange:
  BrowserWindowInterface* browser = GetBrowserWindow();
  auto* tab_list = TabListInterface::From(browser);
  auto* active_tab = tab_list->GetActiveTab();

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser);
  SidePanelType entry_type = entry->type();
  SidePanelEntry* entry_ptr = entry.get();

  auto* registry = SidePanelRegistry::From(active_tab);
  registry->Register(std::move(entry));

  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  coordinator->SetNoDelaysForTesting(true);

  // Act:
  coordinator->SidePanelUIBase::Show(entry_key, /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);

  // Assert:
  std::optional<SidePanelEntry*> active_entry =
      registry->GetActiveEntryFor(entry_type);
  EXPECT_TRUE(active_entry.has_value());
  EXPECT_EQ(entry_ptr, active_entry.value());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       Show_WindowScopedEntry_SetsActiveEntry) {
  // Arrange:
  BrowserWindowInterface* browser = GetBrowserWindow();

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser);
  SidePanelType entry_type = entry->type();
  SidePanelEntry* entry_ptr = entry.get();

  auto* registry = SidePanelRegistry::From(browser);
  registry->Register(std::move(entry));

  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  coordinator->SetNoDelaysForTesting(true);

  // Act:
  coordinator->SidePanelUIBase::Show(entry_key, /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);

  // Assert:
  std::optional<SidePanelEntry*> active_entry =
      registry->GetActiveEntryFor(entry_type);
  EXPECT_TRUE(active_entry.has_value());
  EXPECT_EQ(entry_ptr, active_entry.value());
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    Show_SidePanelAlreadyShownWithDifferentEntry_ReplacesSidePanelContent) {
  // Arrange: Register two entries in the tab-scoped registry.
  BrowserWindowInterface* browser = GetBrowserWindow();
  auto* tab_list = TabListInterface::From(browser);
  auto* active_tab = tab_list->GetActiveTab();
  auto* registry = SidePanelRegistry::From(active_tab);
  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);

  auto first_entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto first_entry = CreateSidePanelEntry(first_entry_key, browser);
  TestSidePanelEntryObserver first_entry_observer;
  first_entry->AddObserver(&first_entry_observer);
  registry->Register(std::move(first_entry));

  auto second_entry_key = SidePanelEntryKey(SidePanelEntryId::kGlic);
  auto second_entry = CreateSidePanelEntry(second_entry_key, browser);
  TestSidePanelEntryObserver second_entry_observer;
  second_entry->AddObserver(&second_entry_observer);
  registry->Register(std::move(second_entry));

  // Arrange: Show the first entry.
  coordinator->SetNoDelaysForTesting(true);
  coordinator->SidePanelUIBase::Show(first_entry_key,
                                     /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);
  ASSERT_TRUE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));

  // Act: Show the second entry.
  coordinator->SidePanelUIBase::Show(second_entry_key,
                                     /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);

  // Assert: Side panel should show second entry.
  EXPECT_FALSE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));
  EXPECT_TRUE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(second_entry_key));

  // Assert: First entry should be notified of "hidden" events.
  EXPECT_EQ(SidePanelEntryHideReason::kReplaced,
            first_entry_observer.reason_for_last_entry_will_hide_.value());
  EXPECT_EQ(first_entry_key.id(),
            first_entry_observer.id_for_last_entry_hidden_.value());
  EXPECT_EQ(
      SidePanelEntryHideReason::kReplaced,
      first_entry_observer.reason_for_last_entry_hidden_with_reason_.value());

  // Assert: Second entry should be notified of the "shown" event.
  EXPECT_EQ(second_entry_key.id(),
            second_entry_observer.id_for_last_entry_shown_.value());
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    Show_SidePanelAlreadyShownWithSameEntry_CancelsLoadingEntryAndKeepsSidePanelOpen) {
  // Arrange:
  BrowserWindowInterface* browser = GetBrowserWindow();

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser);
  TestSidePanelEntryObserver entry_observer;
  entry->AddObserver(&entry_observer);

  auto* registry = SidePanelRegistry::From(browser);
  registry->Register(std::move(entry));

  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  coordinator->SetNoDelaysForTesting(true);

  // Act: Show the entry for the first time.
  coordinator->SidePanelUIBase::Show(entry_key, /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);
  ASSERT_TRUE(coordinator->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));

  // Act: Show the same entry again.
  coordinator->SidePanelUIBase::Show(entry_key, /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);

  // Assert: Side panel should still show the entry.
  EXPECT_TRUE(coordinator->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));

  // Assert: No "hide" events should be triggered.
  EXPECT_FALSE(entry_observer.id_for_last_entry_will_hide_.has_value());
  EXPECT_FALSE(entry_observer.id_for_last_entry_hidden_.has_value());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       Close_TriggersOnEntryWillHideAndOnEntryHidden) {
  // Arrange:
  BrowserWindowInterface* browser = GetBrowserWindow();

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser);
  TestSidePanelEntryObserver entry_observer;
  entry->AddObserver(&entry_observer);

  auto* registry = SidePanelRegistry::From(browser);
  registry->Register(std::move(entry));

  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  coordinator->SetNoDelaysForTesting(true);
  coordinator->SidePanelUIBase::Show(entry_key, /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);
  ASSERT_TRUE(coordinator->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));

  // Act:
  coordinator->Close(SidePanelType::kToolbar,
                     SidePanelEntryHideReason::kSidePanelClosed,
                     /*suppress_animations=*/true);

  // Assert:
  EXPECT_EQ(entry_key.id(),
            entry_observer.id_for_last_entry_will_hide_.value());
  EXPECT_EQ(SidePanelEntryHideReason::kSidePanelClosed,
            entry_observer.reason_for_last_entry_will_hide_.value());
  EXPECT_EQ(entry_key.id(), entry_observer.id_for_last_entry_hidden_.value());
  EXPECT_EQ(entry_key.id(),
            entry_observer.id_for_last_entry_hidden_with_reason_.value());
  EXPECT_EQ(SidePanelEntryHideReason::kSidePanelClosed,
            entry_observer.reason_for_last_entry_hidden_with_reason_.value());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       Close_TabScopedEntry_ResetsActiveEntry) {
  // Arrange:
  BrowserWindowInterface* browser = GetBrowserWindow();
  auto* tab_list = TabListInterface::From(browser);
  auto* active_tab = tab_list->GetActiveTab();

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser);
  SidePanelType entry_type = entry->type();

  auto* registry = SidePanelRegistry::From(active_tab);
  registry->Register(std::move(entry));

  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  coordinator->SetNoDelaysForTesting(true);
  coordinator->SidePanelUIBase::Show(entry_key, /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);
  ASSERT_TRUE(coordinator->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));

  // Act:
  coordinator->Close(entry_type, SidePanelEntryHideReason::kSidePanelClosed,
                     /*suppress_animations=*/true);

  // Assert:
  std::optional<SidePanelEntry*> active_entry =
      registry->GetActiveEntryFor(entry_type);
  EXPECT_FALSE(active_entry.has_value());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       Close_WindowScopedEntry_ResetsActiveEntry) {
  // Arrange:
  BrowserWindowInterface* browser = GetBrowserWindow();

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser);
  SidePanelType entry_type = entry->type();

  auto* registry = SidePanelRegistry::From(browser);
  registry->Register(std::move(entry));

  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  coordinator->SetNoDelaysForTesting(true);
  coordinator->SidePanelUIBase::Show(entry_key, /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);
  ASSERT_TRUE(coordinator->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));

  // Act:
  coordinator->Close(entry_type, SidePanelEntryHideReason::kSidePanelClosed,
                     /*suppress_animations=*/true);

  // Assert:
  std::optional<SidePanelEntry*> active_entry =
      registry->GetActiveEntryFor(entry_type);
  EXPECT_FALSE(active_entry.has_value());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       IsSidePanelEntryShowing_AfterShow_ReturnsTrue) {
  // Arrange:
  BrowserWindowInterface* browser = GetBrowserWindow();

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser);
  TestSidePanelEntryObserver entry_observer;
  entry->AddObserver(&entry_observer);

  auto* registry = SidePanelRegistry::From(browser);
  registry->Register(std::move(entry));

  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  coordinator->SetNoDelaysForTesting(true);
  coordinator->SidePanelUIBase::Show(entry_key, /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);

  // Assert:
  EXPECT_TRUE(coordinator->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       IsSidePanelEntryShowing_AfterClose_ReturnsFalse) {
  // Arrange:
  BrowserWindowInterface* browser = GetBrowserWindow();

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser);
  TestSidePanelEntryObserver entry_observer;
  entry->AddObserver(&entry_observer);

  auto* registry = SidePanelRegistry::From(browser);
  registry->Register(std::move(entry));

  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  coordinator->SetNoDelaysForTesting(true);
  coordinator->SidePanelUIBase::Show(entry_key, /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);
  ASSERT_TRUE(coordinator->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));

  // Act:
  coordinator->Close(SidePanelType::kToolbar,
                     SidePanelEntryHideReason::kSidePanelClosed,
                     /*suppress_animations=*/true);

  // Assert:
  EXPECT_FALSE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    MaybeShowEntryOnTabStripModelChanged_SwitchTabs_NewActiveTabHasNoEntry_ClosesSidePanel) {
  // Arrange: Open 2 tabs.
  BrowserWindowInterface* browser = GetBrowserWindow();
  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  auto* tab_list = TabListInterface::From(browser);
  tabs::TabInterface* first_tab = tab_list->GetActiveTab();
  tabs::TabInterface* second_tab =
      tab_list->OpenTab(GURL("about:blank"), /*index=*/1);
  auto* first_registry = SidePanelRegistry::From(first_tab);
  auto* second_registry = SidePanelRegistry::From(second_tab);
  ASSERT_NE(nullptr, first_registry);
  ASSERT_NE(nullptr, second_registry);
  ASSERT_NE(first_registry, second_registry);
  ASSERT_FALSE(first_tab->IsActivated());
  ASSERT_TRUE(second_tab->IsActivated());

  // Arrange: Register a SidePanelEntry for the 2nd tab.
  auto second_entry_key = SidePanelEntryKey(SidePanelEntryId::kGlic);
  second_registry->Register(CreateSidePanelEntry(second_entry_key, browser));

  // Arrange: Show the SidePanelEntry for the 2nd tab.
  coordinator->SetNoDelaysForTesting(true);
  coordinator->SidePanelUIBase::Show(second_entry_key,
                                     /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);
  ASSERT_TRUE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(second_entry_key));

  // Act: Switch to first tab.
  tab_list->ActivateTab(first_tab->GetHandle());

  // Assert: Side panel should be closed because first tab has no active entry.
  EXPECT_FALSE(coordinator->IsSidePanelShowing(SidePanelType::kToolbar));
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    MaybeShowEntryOnTabStripModelChanged_SwitchTabs_BothTabsHaveActiveEntries_ReplacesSidePanelContent) {
  // Arrange: Open 2 tabs.
  BrowserWindowInterface* browser = GetBrowserWindow();
  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  auto* tab_list = TabListInterface::From(browser);
  tabs::TabInterface* first_tab = tab_list->GetActiveTab();
  tabs::TabInterface* second_tab =
      tab_list->OpenTab(GURL("about:blank"), /*index=*/1);
  ASSERT_FALSE(first_tab->IsActivated());
  ASSERT_TRUE(second_tab->IsActivated());

  // Arrange: Create and register SidePanelEntries for both tabs.
  auto* first_registry = SidePanelRegistry::From(first_tab);
  auto* second_registry = SidePanelRegistry::From(second_tab);
  auto first_entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto second_entry_key = SidePanelEntryKey(SidePanelEntryId::kGlic);

  auto first_entry = CreateSidePanelEntry(first_entry_key, browser);
  TestSidePanelEntryObserver first_entry_observer;
  first_entry->AddObserver(&first_entry_observer);
  first_registry->Register(std::move(first_entry));

  auto second_entry = CreateSidePanelEntry(second_entry_key, browser);
  TestSidePanelEntryObserver second_entry_observer;
  second_entry->AddObserver(&second_entry_observer);
  second_registry->Register(std::move(second_entry));

  // Arrange: Show the SidePanelEntry for the 2nd tab.
  coordinator->SetNoDelaysForTesting(true);
  coordinator->SidePanelUIBase::Show(second_entry_key,
                                     /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);
  ASSERT_TRUE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(second_entry_key));

  // Arrange: Switch to the first tab.
  tab_list->ActivateTab(first_tab->GetHandle());
  ASSERT_FALSE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));

  // Arrange: Show the SidePanelEntry for the first tab.
  coordinator->SidePanelUIBase::Show(first_entry_key,
                                     /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);
  ASSERT_TRUE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));

  // Act: Switch back to second tab.
  tab_list->ActivateTab(second_tab->GetHandle());

  // Assert: Side panel should show second tab's entry (replaces first tab's
  // entry).
  EXPECT_FALSE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));
  EXPECT_TRUE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(second_entry_key));

  // Assert: The first entry should be notified of "hidden" events.
  EXPECT_EQ(SidePanelEntryHideReason::kBackgrounded,
            first_entry_observer.reason_for_last_entry_will_hide_.value());
  EXPECT_EQ(first_entry_key.id(),
            first_entry_observer.id_for_last_entry_hidden_.value());
  EXPECT_EQ(
      SidePanelEntryHideReason::kBackgrounded,
      first_entry_observer.reason_for_last_entry_hidden_with_reason_.value());
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    MaybeShowEntryOnTabStripModelChanged_CloseTab_NewActiveTabHasNoEntry_ClosesSidePanel) {
  // Arrange: Open 2 tabs.
  BrowserWindowInterface* browser = GetBrowserWindow();
  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  auto* tab_list = TabListInterface::From(browser);
  tabs::TabInterface* first_tab = tab_list->GetActiveTab();
  tabs::TabInterface* second_tab =
      tab_list->OpenTab(GURL("about:blank"), /*index=*/1);
  ASSERT_FALSE(first_tab->IsActivated());
  ASSERT_TRUE(second_tab->IsActivated());

  // Arrange: Register a SidePanelEntry for the 2nd tab.
  auto* second_registry = SidePanelRegistry::From(second_tab);
  auto second_entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  second_registry->Register(CreateSidePanelEntry(second_entry_key, browser));

  // Arrange: Show the SidePanelEntry for the 2nd tab.
  coordinator->SetNoDelaysForTesting(true);
  coordinator->SidePanelUIBase::Show(second_entry_key,
                                     /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);
  ASSERT_TRUE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(second_entry_key));

  // Act: Close second tab.
  tab_list->CloseTab(second_tab->GetHandle());

  // Assert: Side panel should be closed.
  EXPECT_FALSE(coordinator->IsSidePanelShowing(SidePanelType::kToolbar));
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    MaybeShowEntryOnTabStripModelChanged_CloseTab_NewActiveTabHasActiveEntry_OpensSidePanel) {
  // Arrange: Open the 1st tab and show an entry.
  BrowserWindowInterface* browser = GetBrowserWindow();
  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  auto* tab_list = TabListInterface::From(browser);
  tabs::TabInterface* first_tab = tab_list->GetActiveTab();
  auto* first_registry = SidePanelRegistry::From(first_tab);
  auto first_entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  first_registry->Register(CreateSidePanelEntry(first_entry_key, browser));
  coordinator->SetNoDelaysForTesting(true);
  coordinator->SidePanelUIBase::Show(first_entry_key,
                                     /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);
  ASSERT_TRUE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));

  // Arrange: Open a 2nd tab.
  // The 1st tab's entry should be closed.
  tabs::TabInterface* second_tab =
      tab_list->OpenTab(GURL("about:blank"), /*index=*/1);
  ASSERT_FALSE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));

  // Act: Close the 2nd tab.
  tab_list->CloseTab(second_tab->GetHandle());

  // Assert: The 1st tab's entry should be shown.
  EXPECT_TRUE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       ShowAndClose_TogglesSidePanel) {
  // Arrange:
  BrowserWindowInterface* browser = GetBrowserWindow();

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser);
  TestSidePanelEntryObserver entry_observer;
  entry->AddObserver(&entry_observer);

  auto* registry = SidePanelRegistry::From(browser);
  registry->Register(std::move(entry));

  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  coordinator->SetNoDelaysForTesting(true);

  // Act: Show
  coordinator->SidePanelUIBase::Show(entry_key,
                                     SidePanelOpenTrigger::kToolbarButton,
                                     /*suppress_animations=*/true);

  // Assert:
  EXPECT_TRUE(coordinator->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));
  EXPECT_EQ(entry_key.id(), entry_observer.id_for_last_entry_shown_.value());

  // Act: Close
  coordinator->Close(SidePanelType::kToolbar,
                     SidePanelEntryHideReason::kSidePanelClosed,
                     /*suppress_animations=*/true);

  // Assert:
  EXPECT_FALSE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));
  EXPECT_EQ(entry_key.id(), entry_observer.id_for_last_entry_hidden_.value());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       Close_CancelsLoadingAndClosesShowingEntry) {
  // Arrange:
  BrowserWindowInterface* browser = GetBrowserWindow();
  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  coordinator->SetNoDelaysForTesting(true);

  auto* registry = SidePanelRegistry::From(browser);

  // 1. Show the first entry (AboutThisSite).
  auto first_entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  registry->Register(CreateSidePanelEntry(first_entry_key, browser));
  coordinator->SidePanelUIBase::Show(first_entry_key,
                                     SidePanelOpenTrigger::kToolbarButton,
                                     /*suppress_animations=*/true);
  ASSERT_TRUE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));

  // 2. Prepare a second entry (Glic) that is not available immediately.
  auto second_entry_key = SidePanelEntryKey(SidePanelEntryId::kGlic);
  SidePanelEntry::CreateContentCallback create_content_callback =
      base::BindRepeating(
          [](BrowserWindowInterface* browser, SidePanelEntryScope& scope) {
            ui::WindowAndroid* window_android =
                browser->GetWindow()->GetNativeWindow();
            base::android::ScopedJavaLocalRef<jobject> java_view =
                Java_SidePanelCoordinatorAndroidBrowserTestSupport_createTestView(
                    base::android::AttachCurrentThread(),
                    window_android->GetJavaObject());
            auto native_view = std::make_unique<SidePanelNativeViewAndroid>(
                base::android::ScopedJavaGlobalRef<jobject>(java_view));
            SidePanelUtil::GetSidePanelContentProxy(native_view.get())
                ->SetAvailable(false);
            return native_view;
          },
          base::Unretained(browser));
  auto default_content_width_callback = base::RepeatingCallback<int()>();
  registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelType::kToolbar, second_entry_key, create_content_callback,
      default_content_width_callback));

  // 3. Act: Start showing the second entry (starts loading).
  coordinator->SetNoDelaysForTesting(false);
  coordinator->SidePanelUIBase::Show(second_entry_key,
                                     SidePanelOpenTrigger::kToolbarButton,
                                     /*suppress_animations=*/true);

  // 4. Assert: First entry is still showing, second is loading.
  EXPECT_TRUE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));
  EXPECT_FALSE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(second_entry_key));
  EXPECT_EQ(registry->GetEntryForKey(second_entry_key),
            coordinator->GetWaiterForTesting(SidePanelType::kToolbar)
                ->loading_entry());

  // 5. Act: Close the side panel.
  coordinator->Close(SidePanelType::kToolbar,
                     SidePanelEntryHideReason::kSidePanelClosed,
                     /*suppress_animations=*/true);

  // 6. Assert: Everything is closed/cancelled.
  EXPECT_FALSE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));
  EXPECT_FALSE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(second_entry_key));
  EXPECT_EQ(nullptr, coordinator->GetWaiterForTesting(SidePanelType::kToolbar)
                         ->loading_entry());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       Show_ReShowsClosingEntry) {
  // Arrange:
  BrowserWindowInterface* browser = GetBrowserWindow();

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser);
  TestSidePanelEntryObserver entry_observer;
  entry->AddObserver(&entry_observer);

  auto* registry = SidePanelRegistry::From(browser);
  registry->Register(std::move(entry));

  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  coordinator->SetNoDelaysForTesting(true);

  // Show the entry first.
  coordinator->SidePanelUIBase::Show(entry_key,
                                     SidePanelOpenTrigger::kToolbarButton,
                                     /*suppress_animations=*/true);
  ASSERT_TRUE(coordinator->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));
  ASSERT_FALSE(coordinator->IsClosing());

  // Act: Starts closing (finishes synchronously when animations are suppressed)
  coordinator->Close(SidePanelType::kToolbar,
                     SidePanelEntryHideReason::kSidePanelClosed,
                     /*suppress_animations=*/true);

  // Assert it's closed (implementation is synchronous for now).
  EXPECT_FALSE(coordinator->IsClosing());
  EXPECT_FALSE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));

  // Act: Should re-show
  coordinator->SidePanelUIBase::Show(entry_key,
                                     SidePanelOpenTrigger::kToolbarButton,
                                     /*suppress_animations=*/true);

  // Assert:
  // It should no longer be closing, and showing again.
  EXPECT_FALSE(coordinator->IsClosing());
  EXPECT_TRUE(coordinator->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       Toggle_ClosedPanel_OpensPanel) {
  // Arrange:
  BrowserWindowInterface* browser = GetBrowserWindow();
  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  coordinator->SetNoDelaysForTesting(true);

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto* registry = SidePanelRegistry::From(browser);
  registry->Register(CreateSidePanelEntry(entry_key, browser));

  ASSERT_FALSE(coordinator->IsSidePanelShowing(SidePanelType::kToolbar));

  // Act:
  coordinator->Toggle(entry_key, SidePanelOpenTrigger::kToolbarButton);

  // Assert:
  EXPECT_TRUE(coordinator->IsSidePanelShowing(SidePanelType::kToolbar));
  EXPECT_TRUE(coordinator->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       Toggle_SameEntry_ClosesPanel) {
  // Arrange:
  BrowserWindowInterface* browser = GetBrowserWindow();
  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  coordinator->SetNoDelaysForTesting(true);

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto* registry = SidePanelRegistry::From(browser);
  registry->Register(CreateSidePanelEntry(entry_key, browser));

  coordinator->SidePanelUIBase::Show(entry_key,
                                     SidePanelOpenTrigger::kToolbarButton,
                                     /*suppress_animations=*/true);
  ASSERT_TRUE(coordinator->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));

  // Act:
  coordinator->Toggle(entry_key, SidePanelOpenTrigger::kToolbarButton);

  // Assert:
  EXPECT_FALSE(coordinator->IsSidePanelShowing(SidePanelType::kToolbar));
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       Toggle_DifferentEntry_ReplacesContent) {
  // Arrange:
  BrowserWindowInterface* browser = GetBrowserWindow();
  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  coordinator->SetNoDelaysForTesting(true);

  auto first_entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto second_entry_key = SidePanelEntryKey(SidePanelEntryId::kGlic);
  auto* registry = SidePanelRegistry::From(browser);
  registry->Register(CreateSidePanelEntry(first_entry_key, browser));
  registry->Register(CreateSidePanelEntry(second_entry_key, browser));

  coordinator->SidePanelUIBase::Show(first_entry_key,
                                     SidePanelOpenTrigger::kToolbarButton,
                                     /*suppress_animations=*/true);
  ASSERT_TRUE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));

  // Act:
  coordinator->Toggle(second_entry_key, SidePanelOpenTrigger::kToolbarButton);

  // Assert:
  EXPECT_TRUE(coordinator->IsSidePanelShowing(SidePanelType::kToolbar));
  EXPECT_FALSE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));
  EXPECT_TRUE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(second_entry_key));
}
