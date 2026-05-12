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
using base::android::AttachCurrentThread;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

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

// Returns a `SidePanelEntry` that can create a test Android View.
//
// `browser` provides the Android Context needed to create the test View.
//
// `on_view_created` is an optional callback for tests to be notified of the
// View.
std::unique_ptr<SidePanelEntry> CreateSidePanelEntry(
    SidePanelEntryKey key,
    BrowserWindowInterface* browser,
    base::RepeatingCallback<void(SidePanelNativeViewAndroid*)> on_view_created =
        {}) {
  SidePanelEntry::CreateContentCallback create_content_callback =
      base::BindRepeating(
          [](BrowserWindowInterface* browser,
             base::RepeatingCallback<void(SidePanelNativeViewAndroid*)>
                 on_view_created,
             SidePanelEntryScope& scope) {
            ui::WindowAndroid* window_android =
                browser->GetWindow()->GetNativeWindow();
            ScopedJavaLocalRef<jobject> java_view =
                Java_SidePanelCoordinatorAndroidBrowserTestSupport_createTestView(
                    AttachCurrentThread(), window_android->GetJavaObject());
            auto native_view = std::make_unique<SidePanelNativeViewAndroid>(
                ScopedJavaGlobalRef<jobject>(java_view));

            if (on_view_created) {
              on_view_created.Run(native_view.get());
            }

            return native_view;
          },
          base::Unretained(browser), std::move(on_view_created));

  return std::make_unique<SidePanelEntry>(
      SidePanelType::kToolbar, key, create_content_callback,
      /*default_content_width_callback=*/base::RepeatingCallback<int()>());
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
  SidePanelEntry* entry_ptr = entry.get();

  auto* registry = SidePanelRegistry::From(active_tab);
  registry->Register(std::move(entry));

  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  coordinator->SetNoDelaysForTesting(true);

  // Act:
  coordinator->SidePanelUIBase::Show(entry_key, /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);

  // Assert:
  std::optional<SidePanelEntry*> active_entry = registry->GetActiveEntry();
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
  SidePanelEntry* entry_ptr = entry.get();

  auto* registry = SidePanelRegistry::From(browser);
  registry->Register(std::move(entry));

  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  coordinator->SetNoDelaysForTesting(true);

  // Act:
  coordinator->SidePanelUIBase::Show(entry_key, /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);

  // Assert:
  std::optional<SidePanelEntry*> active_entry = registry->GetActiveEntry();
  EXPECT_TRUE(active_entry.has_value());
  EXPECT_EQ(entry_ptr, active_entry.value());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       Show_SidePanelNotCurrentlyShown_CachesEntryView) {
  // Arrange:
  BrowserWindowInterface* browser = GetBrowserWindow();

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  ScopedJavaGlobalRef<jobject> entry_java_view;
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser, /*on_view_created=*/
                           base::BindRepeating(
                               [](ScopedJavaGlobalRef<jobject>* java_view,
                                  SidePanelNativeViewAndroid* native_view) {
                                 *java_view = native_view->view();
                               },
                               base::Unretained(&entry_java_view)));
  SidePanelEntry* entry_ptr = entry.get();

  auto* registry = SidePanelRegistry::From(browser);
  registry->Register(std::move(entry));

  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  coordinator->SetNoDelaysForTesting(true);

  // Act:
  coordinator->SidePanelUIBase::Show(entry_key, /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);

  // Assert:
  EXPECT_NE(nullptr, entry_ptr->CachedView().get());
  EXPECT_TRUE(AttachCurrentThread()->IsSameObject(
      entry_java_view.obj(), entry_ptr->CachedView()->view().obj()));
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    Show_SidePanelAlreadyShownWithDifferentEntry_CachesEntryViewForBothPreviousAndCurrentEntries) {
  // Arrange: Register two entries in the tab-scoped registry.
  BrowserWindowInterface* browser = GetBrowserWindow();
  auto* tab_list = TabListInterface::From(browser);
  tabs::TabInterface* active_tab = tab_list->GetActiveTab();
  auto* registry = SidePanelRegistry::From(active_tab);
  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);

  ScopedJavaGlobalRef<jobject> first_java_view;
  ScopedJavaGlobalRef<jobject> second_java_view;
  auto first_entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto second_entry_key = SidePanelEntryKey(SidePanelEntryId::kGlic);
  auto first_entry =
      CreateSidePanelEntry(first_entry_key, browser,
                           /*on_view_created=*/
                           base::BindRepeating(
                               [](ScopedJavaGlobalRef<jobject>* java_view,
                                  SidePanelNativeViewAndroid* native_view) {
                                 *java_view = native_view->view();
                               },
                               base::Unretained(&first_java_view)));
  auto second_entry =
      CreateSidePanelEntry(second_entry_key, browser,
                           /*on_view_created=*/
                           base::BindRepeating(
                               [](ScopedJavaGlobalRef<jobject>* java_view,
                                  SidePanelNativeViewAndroid* native_view) {
                                 *java_view = native_view->view();
                               },
                               base::Unretained(&second_java_view)));
  SidePanelEntry* first_entry_ptr = first_entry.get();
  SidePanelEntry* second_entry_ptr = second_entry.get();
  registry->Register(std::move(first_entry));
  registry->Register(std::move(second_entry));

  // Arrange: Show the first entry.
  coordinator->SetNoDelaysForTesting(true);
  coordinator->SidePanelUIBase::Show(first_entry_key,
                                     /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);

  // Act: Show the second entry.
  coordinator->SidePanelUIBase::Show(second_entry_key,
                                     /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);

  // Assert: Both entries should have cached views.
  JNIEnv* env = AttachCurrentThread();
  EXPECT_TRUE(env->IsSameObject(first_java_view.obj(),
                                first_entry_ptr->CachedView()->view().obj()));
  EXPECT_TRUE(env->IsSameObject(second_java_view.obj(),
                                second_entry_ptr->CachedView()->view().obj()));
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
                       Show_Blocked_WhenWindowTooSmall) {
  // Arrange:
  BrowserWindowInterface* browser = GetBrowserWindow();
  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  coordinator->SetNoDelaysForTesting(true);

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  SidePanelRegistry::From(browser)->Register(
      CreateSidePanelEntry(entry_key, browser));

  // Set window to too small.
  coordinator->OnWindowResized(nullptr, false);

  // Act: Try to show.
  coordinator->SidePanelUIBase::Show(entry_key, std::nullopt, true);

  // Assert: Panel should NOT be showing.
  EXPECT_FALSE(coordinator->IsSidePanelShowing());
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
  coordinator->Close(SidePanelEntryHideReason::kSidePanelClosed,
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

  auto* registry = SidePanelRegistry::From(active_tab);
  registry->Register(std::move(entry));

  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  coordinator->SetNoDelaysForTesting(true);
  coordinator->SidePanelUIBase::Show(entry_key, /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);
  ASSERT_TRUE(coordinator->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));

  // Act:
  coordinator->Close(SidePanelEntryHideReason::kSidePanelClosed,
                     /*suppress_animations=*/true);

  // Assert:
  std::optional<SidePanelEntry*> active_entry = registry->GetActiveEntry();
  EXPECT_FALSE(active_entry.has_value());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       Close_WindowScopedEntry_ResetsActiveEntry) {
  // Arrange:
  BrowserWindowInterface* browser = GetBrowserWindow();

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser);

  auto* registry = SidePanelRegistry::From(browser);
  registry->Register(std::move(entry));

  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  coordinator->SetNoDelaysForTesting(true);
  coordinator->SidePanelUIBase::Show(entry_key, /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);
  ASSERT_TRUE(coordinator->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));

  // Act:
  coordinator->Close(SidePanelEntryHideReason::kSidePanelClosed,
                     /*suppress_animations=*/true);

  // Assert:
  std::optional<SidePanelEntry*> active_entry = registry->GetActiveEntry();
  EXPECT_FALSE(active_entry.has_value());
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    Close_ClearsCachedEntryViewForInactiveEntriesInContextualRegistries) {
  // Arrange: Register two tab-scoped entries.
  BrowserWindowInterface* browser = GetBrowserWindow();
  auto* tab_list = TabListInterface::From(browser);
  tabs::TabInterface* first_tab = tab_list->GetActiveTab();
  tabs::TabInterface* second_tab =
      tab_list->OpenTab(GURL("about:blank"), /*index=*/1);

  auto first_entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto second_entry_key = SidePanelEntryKey(SidePanelEntryId::kGlic);
  std::unique_ptr<SidePanelEntry> first_entry =
      CreateSidePanelEntry(first_entry_key, browser);
  std::unique_ptr<SidePanelEntry> second_entry =
      CreateSidePanelEntry(second_entry_key, browser);
  SidePanelEntry* first_entry_ptr = first_entry.get();
  SidePanelEntry* second_entry_ptr = second_entry.get();

  auto* first_registry = SidePanelRegistry::From(first_tab);
  auto* second_registry = SidePanelRegistry::From(second_tab);
  first_registry->Register(std::move(first_entry));
  second_registry->Register(std::move(second_entry));

  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  coordinator->SetNoDelaysForTesting(true);

  // Arrange:
  // Activate the first tab and show the first entry.
  // Activate the second tab and show the second entry.
  // Go back to the first tab.
  // At this point, all entries should have cached Views.
  tab_list->ActivateTab(first_tab->GetHandle());
  coordinator->SidePanelUIBase::Show(first_entry_key,
                                     /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);
  tab_list->ActivateTab(second_tab->GetHandle());
  coordinator->SidePanelUIBase::Show(second_entry_key,
                                     /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);
  tab_list->ActivateTab(first_tab->GetHandle());
  ASSERT_NE(nullptr, first_entry_ptr->CachedView().get());
  ASSERT_NE(nullptr, second_entry_ptr->CachedView().get());

  // Act: Close the side panel.
  coordinator->Close(SidePanelEntryHideReason::kSidePanelClosed,
                     /*suppress_animations=*/true);

  // Assert: Cached Views for the first entry should be cleared.
  // The second entry should still have its cached View since it's still the
  // active entry in its registry.
  EXPECT_EQ(nullptr, first_entry_ptr->CachedView().get());
  EXPECT_NE(nullptr, second_entry_ptr->CachedView().get());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       Close_ClearsCachedEntryViewForWindowRegistry) {
  // Arrange: Register a window-scoped entry.
  BrowserWindowInterface* browser = GetBrowserWindow();
  auto* registry = SidePanelRegistry::From(browser);

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser);
  SidePanelEntry* entry_ptr = entry.get();
  registry->Register(std::move(entry));

  // Arrange: Show the entry.
  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  coordinator->SetNoDelaysForTesting(true);
  coordinator->SidePanelUIBase::Show(entry_key, /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);
  ASSERT_NE(nullptr, entry_ptr->CachedView().get());

  // Act: Close the side panel.
  coordinator->Close(SidePanelEntryHideReason::kSidePanelClosed,
                     /*suppress_animations=*/true);

  // Assert: The cached view should be cleared.
  EXPECT_EQ(nullptr, entry_ptr->CachedView().get());
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
  coordinator->Close(SidePanelEntryHideReason::kSidePanelClosed,
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
  EXPECT_FALSE(coordinator->IsSidePanelShowing());
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
    MaybeShowEntryOnTabStripModelChanged_SwitchTabs_BothTabsHaveActiveEntries_BothTabsAlsoCallShowOnActiveTabChange_ReplacesSidePanelContent) {
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

  // Arrange: Register tab activation callbacks that call
  // `SidePanelCoordinatorAndroid::Show`.
  // This simulates features like GLiC, which observes tab activation by
  // themselves (see `GlicInstanceImpl::OnBoundTabActivated()`).
  auto first_tab_activation_subscription =
      first_tab->RegisterDidActivate(base::BindRepeating(
          [](SidePanelCoordinatorAndroid* coordinator, SidePanelEntryKey key,
             tabs::TabInterface* tab) {
            coordinator->SidePanelUIBase::Show(key, std::nullopt,
                                               /*suppress_animations=*/true);
          },
          coordinator, first_entry_key));
  auto second_tab_activation_subscription =
      second_tab->RegisterDidActivate(base::BindRepeating(
          [](SidePanelCoordinatorAndroid* coordinator, SidePanelEntryKey key,
             tabs::TabInterface* tab) {
            coordinator->SidePanelUIBase::Show(key, std::nullopt,
                                               /*suppress_animations=*/true);
          },
          coordinator, second_entry_key));

  // Arrange: Show the SidePanelEntry for the 2nd tab.
  coordinator->SetNoDelaysForTesting(true);
  coordinator->SidePanelUIBase::Show(second_entry_key,
                                     /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);
  ASSERT_TRUE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(second_entry_key));

  // Arrange: Switch to the first tab.
  tab_list->ActivateTab(first_tab->GetHandle());
  ASSERT_TRUE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));

  // Act: Switch back to second tab.
  tab_list->ActivateTab(second_tab->GetHandle());

  // Assert: Side panel should show second tab's entry.
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
  EXPECT_FALSE(coordinator->IsSidePanelShowing());
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

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    MaybeShowEntryOnTabStripModelChanged_NullRegistry_DoesNotCrash) {
  // Arrange
  BrowserWindowInterface* browser = GetBrowserWindow();
  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);

  // Act
  // Simulates a tab change to a tab with no WebContents or TabInterface,
  // which causes GetSidePanelRegistryFromWebContents to return nullptr.
  // This verifies that `MaybeShowEntryOnTabStripModelChanged` handles
  // a null registry gracefully.
  coordinator->SidePanelUIBase::OnActiveTabChanged(nullptr, nullptr, false);

  // Assert
  // The fact that this doesn't crash is the primary assertion.
  EXPECT_FALSE(coordinator->IsSidePanelShowing());
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    MaybeShowEntryOnTabStripModelChanged_Blocked_WhenWindowTooSmall) {
  // Arrange: Open 2 tabs, both with their own entries.
  BrowserWindowInterface* browser = GetBrowserWindow();
  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  auto* tab_list = TabListInterface::From(browser);
  tabs::TabInterface* tab_1 = tab_list->GetActiveTab();
  tabs::TabInterface* tab_2 =
      tab_list->OpenTab(GURL("about:blank"), /*index=*/1);

  auto entry_key_1 = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto entry_key_2 = SidePanelEntryKey(SidePanelEntryId::kGlic);
  SidePanelRegistry::From(tab_1)->Register(
      CreateSidePanelEntry(entry_key_1, browser));
  SidePanelRegistry::From(tab_2)->Register(
      CreateSidePanelEntry(entry_key_2, browser));

  coordinator->SetNoDelaysForTesting(true);

  // Show entry in Tab 1.
  tab_list->ActivateTab(tab_1->GetHandle());
  coordinator->SidePanelUIBase::Show(entry_key_1, std::nullopt, true);
  ASSERT_TRUE(coordinator->IsSidePanelShowing());

  // Show entry in Tab 2.
  tab_list->ActivateTab(tab_2->GetHandle());
  coordinator->SidePanelUIBase::Show(entry_key_2, std::nullopt, true);
  ASSERT_TRUE(coordinator->IsSidePanelShowing());

  // Make the window too small. This will hide the panel.
  coordinator->OnWindowResized(nullptr, false);
  ASSERT_FALSE(coordinator->IsSidePanelShowing());

  // Act: Switch to Tab 1 while the window is still small.
  tab_list->ActivateTab(tab_1->GetHandle());

  // Assert: Side panel should NOT be shown even though Tab 1 has an entry.
  EXPECT_FALSE(coordinator->IsSidePanelShowing());
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
  coordinator->Close(SidePanelEntryHideReason::kSidePanelClosed,
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
  auto on_view_created =
      base::BindRepeating([](SidePanelNativeViewAndroid* view) {
        SidePanelUtil::GetSidePanelContentProxy(view)->SetAvailable(false);
      });
  registry->Register(
      CreateSidePanelEntry(second_entry_key, browser, on_view_created));

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
            coordinator->GetWaiterForTesting()->loading_entry());

  // 5. Act: Close the side panel.
  coordinator->Close(SidePanelEntryHideReason::kSidePanelClosed,
                     /*suppress_animations=*/true);

  // 6. Assert: Everything is closed/cancelled.
  EXPECT_FALSE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));
  EXPECT_FALSE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(second_entry_key));
  EXPECT_EQ(nullptr, coordinator->GetWaiterForTesting()->loading_entry());
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
  coordinator->Close(SidePanelEntryHideReason::kSidePanelClosed,
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

  ASSERT_FALSE(coordinator->IsSidePanelShowing());

  // Act:
  coordinator->Toggle(entry_key, SidePanelOpenTrigger::kToolbarButton);

  // Assert:
  EXPECT_TRUE(coordinator->IsSidePanelShowing());
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
  EXPECT_FALSE(coordinator->IsSidePanelShowing());
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
  EXPECT_TRUE(coordinator->IsSidePanelShowing());
  EXPECT_FALSE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));
  EXPECT_TRUE(
      coordinator->SidePanelUIBase::IsSidePanelEntryShowing(second_entry_key));
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    OnWindowResized_True_RestoresActiveEntryIfCachedKeyIsForDifferentTab) {
  // Arrange: Open 2 tabs, both with their own entries.
  BrowserWindowInterface* browser = GetBrowserWindow();
  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  auto* tab_list = TabListInterface::From(browser);
  tabs::TabInterface* tab_1 = tab_list->GetActiveTab();
  tabs::TabInterface* tab_2 =
      tab_list->OpenTab(GURL("about:blank"), /*index=*/1);

  auto entry_key_1 = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto entry_key_2 = SidePanelEntryKey(SidePanelEntryId::kGlic);
  SidePanelRegistry::From(tab_1)->Register(
      CreateSidePanelEntry(entry_key_1, browser));
  SidePanelRegistry::From(tab_2)->Register(
      CreateSidePanelEntry(entry_key_2, browser));

  coordinator->SetNoDelaysForTesting(true);

  // 1. Show entry in Tab 1.
  tab_list->ActivateTab(tab_1->GetHandle());
  coordinator->SidePanelUIBase::Show(entry_key_1, std::nullopt, true);
  ASSERT_TRUE(coordinator->IsSidePanelShowing());

  // 2. Show entry in Tab 2.
  tab_list->ActivateTab(tab_2->GetHandle());
  coordinator->SidePanelUIBase::Show(entry_key_2, std::nullopt, true);
  ASSERT_TRUE(coordinator->IsSidePanelShowing());

  // 3. Make the window too small. This hides the panel and caches Tab 2's key.
  coordinator->OnWindowResized(nullptr, false);
  ASSERT_FALSE(coordinator->IsSidePanelShowing());

  // 4. Switch to Tab 1 while the window is still small.
  // This does NOT show the panel (blocked by small window).
  tab_list->ActivateTab(tab_1->GetHandle());
  ASSERT_FALSE(coordinator->IsSidePanelShowing());

  // 5. Act: Make the window large again.
  coordinator->OnWindowResized(nullptr, true);

  // 6. Assert: Side panel should be shown for Tab 1.
  // Even though the cached key was for Tab 2, Tab 1 has its own active entry.
  EXPECT_TRUE(coordinator->IsSidePanelShowing());
  EXPECT_TRUE(coordinator->IsSidePanelEntryShowing(entry_key_1));
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       OnWindowResized_False_ClosesSidePanel) {
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
  coordinator->OnWindowResized(/*env=*/nullptr,
                               /*should_show_side_panel=*/false);

  // Assert:
  EXPECT_FALSE(coordinator->IsSidePanelShowing());
  EXPECT_EQ(SidePanelEntryHideReason::kWindowResized,
            entry_observer.reason_for_last_entry_hidden_with_reason_.value());

  // Assert: Registry should be reset (consistent with kBackgrounded).
  EXPECT_FALSE(registry->GetActiveEntry().has_value());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       OnWindowResized_True_NoActiveEntry_DoesNothing) {
  // Arrange:
  auto* coordinator = SidePanelCoordinatorAndroid::From(GetBrowserWindow());
  coordinator->SetNoDelaysForTesting(true);

  // Set window to small first.
  coordinator->OnWindowResized(/*env=*/nullptr,
                               /*should_show_side_panel=*/false);
  ASSERT_FALSE(coordinator->IsSidePanelShowing());

  // Act: Make window large again.
  coordinator->OnWindowResized(/*env=*/nullptr,
                               /*should_show_side_panel=*/true);

  // Assert: Panel should stay closed.
  EXPECT_FALSE(coordinator->IsSidePanelShowing());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       OnWindowResized_True_RestoresPreviousEntry) {
  // Arrange:
  BrowserWindowInterface* browser = GetBrowserWindow();

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  SidePanelRegistry::From(browser)->Register(
      CreateSidePanelEntry(entry_key, browser));

  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  coordinator->SetNoDelaysForTesting(true);

  // Show and then hide due to resize.
  coordinator->SidePanelUIBase::Show(entry_key, /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);
  ASSERT_TRUE(coordinator->IsSidePanelShowing());
  coordinator->OnWindowResized(/*env=*/nullptr,
                               /*should_show_side_panel=*/false);
  ASSERT_FALSE(coordinator->IsSidePanelShowing());

  // Act:
  coordinator->OnWindowResized(/*env=*/nullptr,
                               /*should_show_side_panel=*/true);

  // Assert:
  EXPECT_TRUE(coordinator->IsSidePanelShowing());
  EXPECT_TRUE(coordinator->IsSidePanelEntryShowing(entry_key));
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    OnWindowResized_TabIsolation_DoesNotRestoreOnDifferentTab) {
  // Arrange: Open 2 tabs.
  BrowserWindowInterface* browser = GetBrowserWindow();
  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  auto* tab_list = TabListInterface::From(browser);
  tabs::TabInterface* tab_with_entry = tab_list->GetActiveTab();
  tabs::TabInterface* empty_tab =
      tab_list->OpenTab(GURL("about:blank"), /*index=*/1);

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  SidePanelRegistry::From(tab_with_entry)
      ->Register(CreateSidePanelEntry(entry_key, browser));

  coordinator->SetNoDelaysForTesting(true);

  // Activate the tab that has the entry and show it.
  tab_list->ActivateTab(tab_with_entry->GetHandle());
  coordinator->SidePanelUIBase::Show(entry_key,
                                     /*open_trigger=*/std::nullopt,
                                     /*suppress_animations=*/true);
  ASSERT_TRUE(coordinator->IsSidePanelShowing());

  // Hide the panel due to a resize.
  coordinator->OnWindowResized(/*env=*/nullptr,
                               /*should_show_side_panel=*/false);
  ASSERT_FALSE(coordinator->IsSidePanelShowing());

  // Switch to the empty tab.
  tab_list->ActivateTab(empty_tab->GetHandle());

  // Act: Try to "restore" visibility due to resize while on the wrong tab.
  coordinator->OnWindowResized(/*env=*/nullptr,
                               /*should_show_side_panel=*/true);

  // Assert: The panel should NOT restore on the empty tab.
  EXPECT_FALSE(coordinator->IsSidePanelShowing());

  // Act: Switch back to the original tab.
  // This should trigger restoration automatically via
  // MaybeShowEntryOnTabStripModelChanged.
  tab_list->ActivateTab(tab_with_entry->GetHandle());

  // Assert: The panel should now restore correctly on the original tab.
  EXPECT_TRUE(coordinator->IsSidePanelShowing());
  EXPECT_TRUE(coordinator->IsSidePanelEntryShowing(entry_key));
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    OnWindowResized_TabIsolation_SameEntryKeyOnMultipleTabs) {
  // Arrange: Open 2 tabs, both with the SAME entry key registered.
  BrowserWindowInterface* browser = GetBrowserWindow();
  auto* coordinator = SidePanelCoordinatorAndroid::From(browser);
  auto* tab_list = TabListInterface::From(browser);
  tabs::TabInterface* tab_1 = tab_list->GetActiveTab();
  tabs::TabInterface* tab_2 =
      tab_list->OpenTab(GURL("about:blank"), /*index=*/1);

  auto same_entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  SidePanelRegistry::From(tab_1)->Register(
      CreateSidePanelEntry(same_entry_key, browser));
  SidePanelRegistry::From(tab_2)->Register(
      CreateSidePanelEntry(same_entry_key, browser));

  coordinator->SetNoDelaysForTesting(true);

  // 1. Open entry in Tab 1.
  tab_list->ActivateTab(tab_1->GetHandle());
  coordinator->SidePanelUIBase::Show(same_entry_key, std::nullopt, true);
  ASSERT_TRUE(coordinator->IsSidePanelShowing());

  // 2. Window gets small -> Hides.
  coordinator->OnWindowResized(nullptr, false);
  ASSERT_FALSE(coordinator->IsSidePanelShowing());

  // 3. Switch to Tab 2.
  tab_list->ActivateTab(tab_2->GetHandle());

  // 4. Window gets wide again.
  coordinator->OnWindowResized(nullptr, true);

  // Assert: Entry should NOT show in Tab 2 even though it has the same key.
  // This proves that UniqueKey (tab-aware) is used for restoration.
  EXPECT_FALSE(coordinator->IsSidePanelShowing());

  // 5. Open the same entry key in Tab 2 manually.
  coordinator->SidePanelUIBase::Show(same_entry_key, std::nullopt, true);
  ASSERT_TRUE(coordinator->IsSidePanelShowing());
  ASSERT_TRUE(coordinator->IsSidePanelEntryShowing(same_entry_key));

  // 6. Switch back to Tab 1.
  tab_list->ActivateTab(tab_1->GetHandle());

  // Assert: Entry should be restored automatically on Tab 1 because the
  // restoration key was specific to Tab 1 and was never cleared.
  EXPECT_TRUE(coordinator->IsSidePanelShowing());
  EXPECT_TRUE(coordinator->IsSidePanelEntryShowing(same_entry_key));
}
