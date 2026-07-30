// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/internal/android/side_panel_coordinator_android.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
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
#include "chrome/browser/ui/side_panel/test/android/side_panel_android_browser_test_base.h"
#include "components/sessions/core/session_id.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/jni_zero/jni_zero.h"
#include "ui/android/window_android.h"
#include "ui/base/base_window.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/ui/side_panel/test/android/browser_test_support_jni/SidePanelCoordinatorAndroidBrowserTestSupport_jni.h"

namespace {
using jni_zero::AttachCurrentThread;
using jni_zero::ScopedJavaGlobalRef;
using jni_zero::ScopedJavaLocalRef;

// We add a direct `TabModelObserver` to guarantee that the TabModel triggers
// `DidSelectTab` (active tab change) BEFORE `TabRemoved` (tab removal) during
// the reparenting. If this order were reversed, `OnTabReparented` would
// fail to flush the async view detachment (because the replacement hasn't
// started yet), leaving it pending and causing a race condition.
class SequenceEnforcingTabModelObserver : public TabModelObserver {
 public:
  SequenceEnforcingTabModelObserver() = default;
  ~SequenceEnforcingTabModelObserver() override = default;

  void DidSelectTab(TabAndroid* tab, TabModel::TabSelectionType type) override {
    did_select_tab_called_ = true;
  }

  void TabRemoved(TabAndroid* tab) override {
    EXPECT_TRUE(did_select_tab_called_);
    tab_removed_called_ = true;
  }

  bool did_select_tab_called_ = false;
  bool tab_removed_called_ = false;
};

class TestSidePanelEntryObserver final : public SidePanelEntryObserver {
 public:
  explicit TestSidePanelEntryObserver(SidePanelEntry* entry) {
    CHECK(entry);
    observation_.Observe(entry);
  }
  ~TestSidePanelEntryObserver() override = default;

  void OnEntryShown(SidePanelEntry* entry) override {
    id_for_last_entry_shown_ = entry->key().id();
    num_on_entry_shown_received_++;
  }

  void OnEntryWillHide(SidePanelEntry* entry,
                       SidePanelEntryHideReason reason) override {
    id_for_last_entry_will_hide_ = entry->key().id();
    reason_for_last_entry_will_hide_ = reason;
    num_on_entry_will_hide_received_++;
  }

  void OnEntryHidden(SidePanelEntry* entry) override {
    id_for_last_entry_hidden_ = entry->key().id();
    num_on_entry_hidden_received_++;
  }

  void OnEntryHiddenWithReason(SidePanelEntry* entry,
                               SidePanelEntryHideReason reason) override {
    id_for_last_entry_hidden_with_reason_ = entry->key().id();
    reason_for_last_entry_hidden_with_reason_ = reason;
    num_on_entry_hidden_with_reason_received_++;
  }

  void OnEntryHideCancelled(SidePanelEntry* entry) override {
    id_for_last_entry_hide_cancelled_ = entry->key().id();
    num_on_entry_hide_cancelled_received_++;
  }

  std::optional<SidePanelEntry::Id> id_for_last_entry_shown_;

  std::optional<SidePanelEntry::Id> id_for_last_entry_will_hide_;
  std::optional<SidePanelEntryHideReason> reason_for_last_entry_will_hide_;

  std::optional<SidePanelEntry::Id> id_for_last_entry_hidden_;

  std::optional<SidePanelEntry::Id> id_for_last_entry_hidden_with_reason_;
  std::optional<SidePanelEntryHideReason>
      reason_for_last_entry_hidden_with_reason_;

  std::optional<SidePanelEntry::Id> id_for_last_entry_hide_cancelled_;

  int num_on_entry_shown_received_ = 0;
  int num_on_entry_will_hide_received_ = 0;
  int num_on_entry_hidden_received_ = 0;
  int num_on_entry_hidden_with_reason_received_ = 0;
  int num_on_entry_hide_cancelled_received_ = 0;

 private:
  base::ScopedObservation<SidePanelEntry, SidePanelEntryObserver> observation_{
      this};
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
    bool use_thin_web_view = false,
    base::RepeatingCallback<void(SidePanelNativeViewAndroid*)> on_view_created =
        {}) {
  SidePanelEntry::CreateContentCallback create_content_callback =
      base::BindRepeating(
          [](BrowserWindowInterface* browser, bool use_thin_web_view,
             base::RepeatingCallback<void(SidePanelNativeViewAndroid*)>
                 on_view_created,
             SidePanelEntryScope& scope) {
            ui::WindowAndroid* window_android =
                browser->GetWindow()->GetNativeWindow();
            ScopedJavaLocalRef<jobject> java_view =
                Java_SidePanelCoordinatorAndroidBrowserTestSupport_createTestView(
                    AttachCurrentThread(), browser->GetProfile(),
                    window_android, use_thin_web_view);
            auto native_view =
                std::make_unique<SidePanelNativeViewAndroid>(java_view);

            if (on_view_created) {
              on_view_created.Run(native_view.get());
            }

            return native_view;
          },
          base::Unretained(browser), use_thin_web_view,
          std::move(on_view_created));

  auto entry = std::make_unique<SidePanelEntry>(
      SidePanelType::kToolbar, key, create_content_callback,
      /*default_content_width_callback=*/base::RepeatingCallback<int()>());
  entry->SetProperty(kSidePanelTitleKey, std::u16string(u"Test Panel"));
  return entry;
}

class AutoOpenSidePanelTabModelObserver : public TabModelObserver {
 public:
  AutoOpenSidePanelTabModelObserver(BrowserWindowInterface* browser,
                                    SidePanelCoordinatorAndroid* coordinator)
      : browser_(browser), coordinator_(coordinator) {}
  ~AutoOpenSidePanelTabModelObserver() override = default;

  void DidSelectTab(TabAndroid* tab, TabModel::TabSelectionType type) override {
    auto key = SidePanelEntryKey(SidePanelEntryId::kGlic);
    auto* registry = SidePanelRegistry::From(tab);
    if (!registry->GetEntryForKey(key)) {
      registry->Register(
          CreateSidePanelEntry(key, browser_, /*use_thin_web_view=*/true));
      side_panel_entry_observers_.push_back(
          std::make_unique<TestSidePanelEntryObserver>(
              registry->GetEntryForKey(key)));
    }
    coordinator_->SidePanelUIBase::Show(key,
                                        SidePanelOpenTrigger::kToolbarButton,
                                        /*suppress_animations=*/true);
  }

  const std::vector<std::unique_ptr<TestSidePanelEntryObserver>>&
  side_panel_entry_observers() const {
    return side_panel_entry_observers_;
  }

 private:
  raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<SidePanelCoordinatorAndroid> coordinator_;
  std::vector<std::unique_ptr<TestSidePanelEntryObserver>>
      side_panel_entry_observers_;
};

BrowserWindowInterface* CreateBrowserWindowAsync(Profile* profile) {
  BrowserWindowCreateParams create_params = BrowserWindowCreateParams(
      BrowserWindowInterface::Type::TYPE_NORMAL, *profile,
      /*from_user_gesture=*/false);
  base::test::TestFuture<BrowserWindowInterface*> future;
  CreateBrowserWindow(std::move(create_params), future.GetCallback());
  return future.Get();
}

// Wait until both the Java layout and the C++ state reflect that the side panel
// is open.
void WaitUntilOpened(SidePanelCoordinatorAndroid* coordinator) {
  ASSERT_TRUE(base::test::RunUntil([coordinator]() {
    return coordinator->GetContainerWidthForTesting() > 0 &&
           coordinator->GetStateForTesting() == SidePanelState::kShown;
  }));
}

// Wait until both the Java layout and the C++ state reflect that the side panel
// is closed.
void WaitUntilClosed(SidePanelCoordinatorAndroid* coordinator) {
  ASSERT_TRUE(base::test::RunUntil([coordinator]() {
    return coordinator->GetContainerWidthForTesting() == 0 &&
           coordinator->GetStateForTesting() == SidePanelState::kClosed;
  }));
}
}  // namespace

class SidePanelCoordinatorAndroidBrowserTest
    : public SidePanelAndroidBrowserTestBase {
 protected:
  using SidePanelAndroidBrowserTestBase::GetActiveTabInLastActiveBrowser;
  using SidePanelAndroidBrowserTestBase::GetLastActiveBrowser;

  void TearDownOnMainThread() override {
    std::vector<BrowserWindowInterface*> windows =
        GetAllBrowserWindowInterfaces();
    for (size_t i = 1; i < windows.size(); ++i) {
      if (ui::BaseWindow* base_window = windows[i]->GetWindow()) {
        base_window->Close();
      }
    }
    SidePanelAndroidBrowserTestBase::TearDownOnMainThread();
  }

  /**
   * Sets up tab-scoped and/or window-scoped side panel entries for the given
   * browser window.
   *
   * @param window The browser window interface to configure. Must not be null.
   * @param window_scoped_entry_id Optional entry ID to register as a
   * window-scoped side panel entry. If provided, it will be registered and
   * immediately shown.
   * @param tab_scoped_entry_ids A list of optional entry IDs to register for
   * each tab in the window. The index in the vector maps to the tab's position
   * in the tab strip. If a tab contains a value, the entry will be registered
   * and shown for that tab.
   * @param active_tab_index The index of the tab to activate at the end of
   * setup. Must be within the bounds of tab_scoped_entry_ids.
   */
  void SetUpSidePanelEntriesForWindow(
      BrowserWindowInterface* window,
      std::optional<SidePanelEntryId> window_scoped_entry_id,
      std::vector<std::optional<SidePanelEntryId>> tab_scoped_entry_ids,
      int active_tab_index,
      bool use_thin_web_view = false) {
    CHECK(window);
    if (!tab_scoped_entry_ids.empty()) {
      CHECK_GE(active_tab_index, 0);
      CHECK_LT(active_tab_index, static_cast<int>(tab_scoped_entry_ids.size()));
    }

    auto* tab_list = TabListInterface::From(window);
    auto* coordinator = SidePanelCoordinatorAndroid::From(window);

    // Pre-create tabs while the panel is closed so they don't inherit the open
    // panel state.
    while (tab_list->GetTabCount() <
           static_cast<int>(tab_scoped_entry_ids.size())) {
      tab_list->OpenTab(GURL("about:blank"), tab_list->GetTabCount());
    }

    // Open the default window-scoped panel if requested.
    if (window_scoped_entry_id.has_value()) {
      auto key = SidePanelEntryKey(window_scoped_entry_id.value());
      SidePanelRegistry::From(window)->Register(
          CreateSidePanelEntry(key, window, use_thin_web_view));
      coordinator->SidePanelUIBase::Show(key,
                                         SidePanelOpenTrigger::kToolbarButton,
                                         /*suppress_animations=*/true);
      WaitUntilOpened(coordinator);
    }

    // Initialize the active side panel state for each tab individually.
    for (size_t i = 0; i < tab_scoped_entry_ids.size(); ++i) {
      if (tab_scoped_entry_ids[i].has_value()) {
        tabs::TabInterface* tab = tab_list->GetTab(i);
        bool was_showing = coordinator->IsSidePanelShowing();

        // Switch to the tab. If the panel was showing, it will automatically
        // close asynchronously (unless a window-scoped panel keeps it open). We
        // must wait for it to fully close before calling Show() to avoid
        // transition collisions.
        tab_list->ActivateTab(tab->GetHandle());
        if (was_showing && !window_scoped_entry_id.has_value()) {
          WaitUntilClosed(coordinator);
        }

        auto key = SidePanelEntryKey(tab_scoped_entry_ids[i].value());
        SidePanelRegistry::From(tab)->Register(
            CreateSidePanelEntry(key, window, use_thin_web_view));
        coordinator->SidePanelUIBase::Show(key,
                                           SidePanelOpenTrigger::kToolbarButton,
                                           /*suppress_animations=*/true);

        // Wait for it to open so the active state is saved in the tab's
        // registry.
        WaitUntilOpened(coordinator);
      }
    }

    // Activate the final target tab and wait for its panel to restore or close.
    // Guarded by !empty() to avoid out-of-bounds access if only window-scoped
    // entries exist.
    if (!tab_scoped_entry_ids.empty()) {
      tab_list->ActivateTab(tab_list->GetTab(active_tab_index)->GetHandle());
      if (tab_scoped_entry_ids[active_tab_index].has_value() ||
          window_scoped_entry_id.has_value()) {
        WaitUntilOpened(coordinator);
      } else {
        WaitUntilClosed(coordinator);
      }
    }
  }

  void SetUpOnMainThread() override {
    SidePanelAndroidBrowserTestBase::SetUpOnMainThread();
    browser_ = GetLastActiveBrowser();
    tab_list_ = TabListInterface::From(browser_);
    coordinator_ = SidePanelCoordinatorAndroid::From(browser_);
    coordinator_->SetNoDelaysForTesting(true);
  }

  raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<TabListInterface> tab_list_;
  raw_ptr<SidePanelCoordinatorAndroid> coordinator_;
};

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       TestSidePanelUIProvider) {
  SidePanelUI* side_panel_ui =
      SidePanelUIProvider::From(GetLastActiveBrowser());
  EXPECT_NE(nullptr, side_panel_ui);
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       Show_TriggersOnEntryShown) {
  // Arrange:

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser_);
  TestSidePanelEntryObserver entry_observer(entry.get());

  auto* registry = SidePanelRegistry::From(browser_);
  registry->Register(std::move(entry));

  // Act:
  // SidePanelCoordinatorAndroid::Show(const UniqueKey&,
  // std::optional<SidePanelOpenTrigger>, bool) is protected, so we use
  // SidePanelUIBase to call SidePanelCoordinatorAndroid::Show().
  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);

  // Assert:
  EXPECT_EQ(entry_key.id(), entry_observer.id_for_last_entry_shown_.value());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       Show_TabScopedEntry_SetsActiveEntry) {
  // Arrange:
  auto* active_tab = tab_list_->GetActiveTab();

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser_);
  SidePanelEntry* entry_ptr = entry.get();

  auto* registry = SidePanelRegistry::From(active_tab);
  registry->Register(std::move(entry));

  // Act:
  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);

  // Assert:
  std::optional<SidePanelEntry*> active_entry = registry->GetActiveEntry();
  EXPECT_TRUE(active_entry.has_value());
  EXPECT_EQ(entry_ptr, active_entry.value());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       Show_WindowScopedEntry_SetsActiveEntry) {
  // Arrange:

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser_);
  SidePanelEntry* entry_ptr = entry.get();

  auto* registry = SidePanelRegistry::From(browser_);
  registry->Register(std::move(entry));

  // Act:
  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);

  // Assert:
  std::optional<SidePanelEntry*> active_entry = registry->GetActiveEntry();
  EXPECT_TRUE(active_entry.has_value());
  EXPECT_EQ(entry_ptr, active_entry.value());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       Show_SidePanelNotCurrentlyShown_CachesEntryView) {
  // Arrange:

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  ScopedJavaGlobalRef<jobject> entry_java_view;
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser_,
                           /*use_thin_web_view=*/false,
                           /*on_view_created=*/
                           base::BindRepeating(
                               [](ScopedJavaGlobalRef<jobject>* java_view,
                                  SidePanelNativeViewAndroid* native_view) {
                                 *java_view = native_view->view();
                               },
                               base::Unretained(&entry_java_view)));
  SidePanelEntry* entry_ptr = entry.get();

  auto* registry = SidePanelRegistry::From(browser_);
  registry->Register(std::move(entry));

  // Act:
  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);

  // Assert:
  EXPECT_NE(nullptr, entry_ptr->CachedView().get());
  EXPECT_TRUE(AttachCurrentThread()->IsSameObject(
      entry_java_view.obj(), entry_ptr->CachedView()->view().obj()));
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       Destructor_ClearsCachedEntryViews) {
  // Arrange: Register entries in window and tab registries and populate cache.
  tabs::TabInterface* active_tab = tab_list_->GetActiveTab();
  auto window_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto tab_key = SidePanelEntryKey(SidePanelEntryId::kGlic);

  auto window_entry = CreateSidePanelEntry(window_key, browser_);
  auto tab_entry = CreateSidePanelEntry(tab_key, browser_);
  SidePanelEntry* window_entry_ptr = window_entry.get();
  SidePanelEntry* tab_entry_ptr = tab_entry.get();

  SidePanelRegistry::From(browser_)->Register(std::move(window_entry));
  SidePanelRegistry::From(active_tab)->Register(std::move(tab_entry));

  coordinator_->SidePanelUIBase::Show(window_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  coordinator_->SidePanelUIBase::Show(tab_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);

  ASSERT_NE(nullptr, window_entry_ptr->CachedView().get());
  ASSERT_NE(nullptr, tab_entry_ptr->CachedView().get());

  // Act: Destroy the coordinator (simulating window/activity destruction).
  delete coordinator_.ExtractAsDangling();

  // Assert: Cached views in both window and tab registries are cleared...
  EXPECT_EQ(nullptr, window_entry_ptr->CachedView().get());
  EXPECT_EQ(nullptr, tab_entry_ptr->CachedView().get());

  // ...but active entry selection remains preserved for window restoration.
  EXPECT_EQ(
      tab_entry_ptr,
      SidePanelRegistry::From(active_tab)->GetActiveEntry().value_or(nullptr));
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    Show_SidePanelAlreadyShownWithDifferentEntry_CachesEntryViewForBothPreviousAndCurrentEntries) {
  // Arrange: Register two entries in the tab-scoped registry.
  tabs::TabInterface* active_tab = tab_list_->GetActiveTab();
  auto* registry = SidePanelRegistry::From(active_tab);

  ScopedJavaGlobalRef<jobject> first_java_view;
  ScopedJavaGlobalRef<jobject> second_java_view;
  auto first_entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto second_entry_key = SidePanelEntryKey(SidePanelEntryId::kGlic);
  auto first_entry =
      CreateSidePanelEntry(first_entry_key, browser_,
                           /*use_thin_web_view=*/false,
                           /*on_view_created=*/
                           base::BindRepeating(
                               [](ScopedJavaGlobalRef<jobject>* java_view,
                                  SidePanelNativeViewAndroid* native_view) {
                                 *java_view = native_view->view();
                               },
                               base::Unretained(&first_java_view)));
  auto second_entry =
      CreateSidePanelEntry(second_entry_key, browser_,
                           /*use_thin_web_view=*/false,
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
  coordinator_->SidePanelUIBase::Show(first_entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);

  // Act: Show the second entry.
  coordinator_->SidePanelUIBase::Show(second_entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);

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
  auto* active_tab = tab_list_->GetActiveTab();
  auto* registry = SidePanelRegistry::From(active_tab);

  auto first_entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto first_entry = CreateSidePanelEntry(first_entry_key, browser_);
  TestSidePanelEntryObserver first_entry_observer(first_entry.get());
  registry->Register(std::move(first_entry));

  auto second_entry_key = SidePanelEntryKey(SidePanelEntryId::kGlic);
  auto second_entry = CreateSidePanelEntry(second_entry_key, browser_);
  TestSidePanelEntryObserver second_entry_observer(second_entry.get());
  registry->Register(std::move(second_entry));

  // Arrange: Show the first entry.
  coordinator_->SidePanelUIBase::Show(first_entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));

  // Act: Show the second entry.
  coordinator_->SidePanelUIBase::Show(second_entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);

  // Assert: Side panel should show second entry.
  EXPECT_FALSE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));
  EXPECT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(second_entry_key));

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

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser_);
  TestSidePanelEntryObserver entry_observer(entry.get());

  auto* registry = SidePanelRegistry::From(browser_);
  registry->Register(std::move(entry));

  // Act: Show the entry for the first time.
  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));

  // Act: Show the same entry again.
  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);

  // Assert: Side panel should still show the entry.
  EXPECT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));

  // Assert: No "hide" events should be triggered.
  EXPECT_FALSE(entry_observer.id_for_last_entry_will_hide_.has_value());
  EXPECT_FALSE(entry_observer.id_for_last_entry_hidden_.has_value());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       Show_InsufficientSpace_Blocked) {
  // Arrange:
  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  SidePanelRegistry::From(browser_)->Register(
      CreateSidePanelEntry(entry_key, browser_));

  // Arrange: Make the space insufficient.
  coordinator_->OnWillAutoClose();

  // Act: Try to show.
  coordinator_->SidePanelUIBase::Show(
      entry_key, SidePanelOpenTrigger::kToolbarButton, true);

  // Assert: Panel should NOT be showing.
  EXPECT_FALSE(coordinator_->IsSidePanelShowing());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       Close_TriggersOnEntryWillHideAndOnEntryHidden) {
  // Arrange:

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser_);
  TestSidePanelEntryObserver entry_observer(entry.get());

  auto* registry = SidePanelRegistry::From(browser_);
  registry->Register(std::move(entry));

  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));

  // Act:
  coordinator_->Close(SidePanelEntryHideReason::kSidePanelClosed,
                      /*suppress_animations=*/true);
  WaitUntilClosed(coordinator_);

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
  auto* active_tab = tab_list_->GetActiveTab();

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser_);

  auto* registry = SidePanelRegistry::From(active_tab);
  registry->Register(std::move(entry));

  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));

  // Act:
  coordinator_->Close(SidePanelEntryHideReason::kSidePanelClosed,
                      /*suppress_animations=*/true);
  WaitUntilClosed(coordinator_);

  // Assert:
  std::optional<SidePanelEntry*> active_entry = registry->GetActiveEntry();
  EXPECT_FALSE(active_entry.has_value());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       Close_WindowScopedEntry_ResetsActiveEntry) {
  // Arrange:

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser_);

  auto* registry = SidePanelRegistry::From(browser_);
  registry->Register(std::move(entry));

  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));

  // Act:
  coordinator_->Close(SidePanelEntryHideReason::kSidePanelClosed,
                      /*suppress_animations=*/true);
  WaitUntilClosed(coordinator_);

  // Assert:
  std::optional<SidePanelEntry*> active_entry = registry->GetActiveEntry();
  EXPECT_FALSE(active_entry.has_value());
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    Close_ClearsCachedEntryViewForInactiveEntriesInContextualRegistries) {
  // Arrange: Register two tab-scoped entries.
  tabs::TabInterface* first_tab = tab_list_->GetActiveTab();
  tabs::TabInterface* second_tab =
      tab_list_->OpenTab(GURL("about:blank"), /*index=*/1);

  auto first_entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto second_entry_key = SidePanelEntryKey(SidePanelEntryId::kGlic);
  std::unique_ptr<SidePanelEntry> first_entry =
      CreateSidePanelEntry(first_entry_key, browser_);
  std::unique_ptr<SidePanelEntry> second_entry =
      CreateSidePanelEntry(second_entry_key, browser_);
  SidePanelEntry* first_entry_ptr = first_entry.get();
  SidePanelEntry* second_entry_ptr = second_entry.get();

  auto* first_registry = SidePanelRegistry::From(first_tab);
  auto* second_registry = SidePanelRegistry::From(second_tab);
  first_registry->Register(std::move(first_entry));
  second_registry->Register(std::move(second_entry));

  // Arrange:
  // Activate the first tab and show the first entry.
  // Activate the second tab and show the second entry.
  // Go back to the first tab.
  // At this point, all entries should have cached Views.
  tab_list_->ActivateTab(first_tab->GetHandle());
  coordinator_->SidePanelUIBase::Show(first_entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  tab_list_->ActivateTab(second_tab->GetHandle());
  WaitUntilClosed(coordinator_);
  coordinator_->SidePanelUIBase::Show(second_entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  tab_list_->ActivateTab(first_tab->GetHandle());
  WaitUntilOpened(coordinator_);
  ASSERT_NE(nullptr, first_entry_ptr->CachedView().get());
  ASSERT_NE(nullptr, second_entry_ptr->CachedView().get());

  // Act: Close the side panel.
  coordinator_->Close(SidePanelEntryHideReason::kSidePanelClosed,
                      /*suppress_animations=*/true);
  WaitUntilClosed(coordinator_);

  // Assert: Cached Views for the first entry should be cleared.
  // The second entry should still have its cached View since it's still the
  // active entry in its registry.
  EXPECT_EQ(nullptr, first_entry_ptr->CachedView().get());
  EXPECT_NE(nullptr, second_entry_ptr->CachedView().get());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       Close_ClearsCachedEntryViewForWindowRegistry) {
  // Arrange: Register a window-scoped entry.
  auto* registry = SidePanelRegistry::From(browser_);

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser_);
  SidePanelEntry* entry_ptr = entry.get();
  registry->Register(std::move(entry));

  // Arrange: Show the entry.
  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_NE(nullptr, entry_ptr->CachedView().get());

  // Act: Close the side panel.
  coordinator_->Close(SidePanelEntryHideReason::kSidePanelClosed,
                      /*suppress_animations=*/true);
  WaitUntilClosed(coordinator_);

  // Assert: The cached view should be cleared.
  EXPECT_EQ(nullptr, entry_ptr->CachedView().get());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       IsSidePanelEntryShowing_AfterShow_ReturnsTrue) {
  // Arrange:

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser_);
  TestSidePanelEntryObserver entry_observer(entry.get());

  auto* registry = SidePanelRegistry::From(browser_);
  registry->Register(std::move(entry));

  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);

  // Assert:
  EXPECT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       IsSidePanelEntryShowing_AfterClose_ReturnsFalse) {
  // Arrange:

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser_);
  TestSidePanelEntryObserver entry_observer(entry.get());

  auto* registry = SidePanelRegistry::From(browser_);
  registry->Register(std::move(entry));

  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));

  // Act:
  coordinator_->Close(SidePanelEntryHideReason::kSidePanelClosed,
                      /*suppress_animations=*/true);
  WaitUntilClosed(coordinator_);

  // Assert:
  EXPECT_FALSE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    MaybeShowEntryOnTabStripModelChanged_SwitchTabs_NewActiveTabHasNoEntry_ClosesSidePanel) {
  // Arrange: Open 2 tabs.
  tabs::TabInterface* first_tab = tab_list_->GetActiveTab();
  tabs::TabInterface* second_tab =
      tab_list_->OpenTab(GURL("about:blank"), /*index=*/1);
  auto* first_registry = SidePanelRegistry::From(first_tab);
  auto* second_registry = SidePanelRegistry::From(second_tab);
  ASSERT_NE(nullptr, first_registry);
  ASSERT_NE(nullptr, second_registry);
  ASSERT_NE(first_registry, second_registry);
  ASSERT_FALSE(first_tab->IsActivated());
  ASSERT_TRUE(second_tab->IsActivated());

  // Arrange: Register a SidePanelEntry for the 2nd tab.
  auto second_entry_key = SidePanelEntryKey(SidePanelEntryId::kGlic);
  second_registry->Register(CreateSidePanelEntry(second_entry_key, browser_));

  // Arrange: Show the SidePanelEntry for the 2nd tab.
  coordinator_->SidePanelUIBase::Show(second_entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(second_entry_key));

  // Act: Switch to first tab.
  tab_list_->ActivateTab(first_tab->GetHandle());
  WaitUntilClosed(coordinator_);

  // Assert: Side panel should be closed because first tab has no active entry.
  EXPECT_FALSE(coordinator_->IsSidePanelShowing());
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    MaybeShowEntryOnTabStripModelChanged_SwitchTabs_BothTabsHaveActiveEntries_ReplacesSidePanelContent) {
  // Arrange: Open 2 tabs.
  tabs::TabInterface* first_tab = tab_list_->GetActiveTab();
  tabs::TabInterface* second_tab =
      tab_list_->OpenTab(GURL("about:blank"), /*index=*/1);
  ASSERT_FALSE(first_tab->IsActivated());
  ASSERT_TRUE(second_tab->IsActivated());

  // Arrange: Create and register SidePanelEntries for both tabs.
  auto* first_registry = SidePanelRegistry::From(first_tab);
  auto* second_registry = SidePanelRegistry::From(second_tab);
  auto first_entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto second_entry_key = SidePanelEntryKey(SidePanelEntryId::kGlic);

  auto first_entry = CreateSidePanelEntry(first_entry_key, browser_);
  TestSidePanelEntryObserver first_entry_observer(first_entry.get());
  first_registry->Register(std::move(first_entry));

  auto second_entry = CreateSidePanelEntry(second_entry_key, browser_);
  TestSidePanelEntryObserver second_entry_observer(second_entry.get());
  second_registry->Register(std::move(second_entry));

  // Arrange: Show the SidePanelEntry for the 2nd tab.
  coordinator_->SidePanelUIBase::Show(second_entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(second_entry_key));

  // Arrange: Switch to the first tab.
  tab_list_->ActivateTab(first_tab->GetHandle());
  WaitUntilClosed(coordinator_);
  ASSERT_FALSE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));

  // Arrange: Show the SidePanelEntry for the first tab.
  coordinator_->SidePanelUIBase::Show(first_entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));

  // Act: Switch back to second tab.
  tab_list_->ActivateTab(second_tab->GetHandle());
  WaitUntilOpened(coordinator_);

  // Assert: Side panel should show second tab's entry (replaces first tab's
  // entry).
  EXPECT_FALSE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));
  EXPECT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(second_entry_key));

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
    MaybeShowEntryOnTabStripModelChanged_SwitchTabs_BothTabsHaveActiveEntries_EnableDeferredViewReplacement_ReplacesSidePanelContent) {
  // Arrange: Open 2 tabs.
  tabs::TabInterface* first_tab = tab_list_->GetActiveTab();
  tabs::TabInterface* second_tab =
      tab_list_->OpenTab(GURL("about:blank"), /*index=*/1);
  ASSERT_FALSE(first_tab->IsActivated());
  ASSERT_TRUE(second_tab->IsActivated());

  // Arrange: Create and register SidePanelEntries for both tabs.
  //
  // Note that the entry for the 2nd tab uses a ThinWebView, which is required
  // to test deferred View replacement when we switch from the 1st tab to the
  // 2nd tab.
  auto* first_registry = SidePanelRegistry::From(first_tab);
  auto* second_registry = SidePanelRegistry::From(second_tab);
  auto first_entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto second_entry_key = SidePanelEntryKey(SidePanelEntryId::kGlic);

  auto first_entry = CreateSidePanelEntry(first_entry_key, browser_);
  TestSidePanelEntryObserver first_entry_observer(first_entry.get());
  first_registry->Register(std::move(first_entry));

  auto second_entry = CreateSidePanelEntry(second_entry_key, browser_,
                                           /*use_thin_web_view=*/true);
  TestSidePanelEntryObserver second_entry_observer(second_entry.get());
  second_registry->Register(std::move(second_entry));

  // Arrange: Show the SidePanelEntry for the 2nd tab.
  coordinator_->SidePanelUIBase::Show(second_entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(second_entry_key));

  // Arrange: Switch to the first tab.
  tab_list_->ActivateTab(first_tab->GetHandle());
  WaitUntilClosed(coordinator_);
  ASSERT_FALSE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));

  // Arrange: Show the SidePanelEntry for the first tab.
  coordinator_->SidePanelUIBase::Show(first_entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));

  // Act: Switch back to second tab. This should cause a content View
  // replacement.
  //
  // Note that since we enabled the deferred View replacement, we should wait
  // until there is no "pending replaced entry".
  coordinator_->ConfigDeferredViewReplacementForTesting(true);
  tab_list_->ActivateTab(second_tab->GetHandle());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !coordinator_->HasPendingReplacedEntryForTesting(); }));
  WaitUntilOpened(coordinator_);

  // Assert: Side panel should show second tab's entry (replaces first tab's
  // entry).
  EXPECT_FALSE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));
  EXPECT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(second_entry_key));

  // Assert: The first entry should be notified of "hidden" events.
  EXPECT_EQ(SidePanelEntryHideReason::kBackgrounded,
            first_entry_observer.reason_for_last_entry_will_hide_.value());
  EXPECT_EQ(first_entry_key.id(),
            first_entry_observer.id_for_last_entry_hidden_.value());
  EXPECT_EQ(
      SidePanelEntryHideReason::kBackgrounded,
      first_entry_observer.reason_for_last_entry_hidden_with_reason_.value());

  // Clean up:
  coordinator_->ConfigDeferredViewReplacementForTesting(false);
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    MaybeShowEntryOnTabStripModelChanged_SwitchTabs_BothTabsHaveActiveEntries_BothTabsAlsoCallShowOnActiveTabChange_ReplacesSidePanelContent) {
  // Arrange: Open 2 tabs.
  tabs::TabInterface* first_tab = tab_list_->GetActiveTab();
  tabs::TabInterface* second_tab =
      tab_list_->OpenTab(GURL("about:blank"), /*index=*/1);
  ASSERT_FALSE(first_tab->IsActivated());
  ASSERT_TRUE(second_tab->IsActivated());

  // Arrange: Create and register SidePanelEntries for both tabs.
  auto* first_registry = SidePanelRegistry::From(first_tab);
  auto* second_registry = SidePanelRegistry::From(second_tab);
  auto first_entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto second_entry_key = SidePanelEntryKey(SidePanelEntryId::kGlic);

  auto first_entry = CreateSidePanelEntry(first_entry_key, browser_);
  TestSidePanelEntryObserver first_entry_observer(first_entry.get());
  first_registry->Register(std::move(first_entry));

  auto second_entry = CreateSidePanelEntry(second_entry_key, browser_);
  TestSidePanelEntryObserver second_entry_observer(second_entry.get());
  second_registry->Register(std::move(second_entry));

  // Arrange: Register tab activation callbacks that call
  // `SidePanelCoordinatorAndroid::Show`.
  // This simulates features like GLiC, which observes tab activation by
  // themselves (see `GlicInstanceImpl::OnBoundTabActivated()`).
  auto first_tab_activation_subscription =
      first_tab->RegisterDidActivate(base::BindRepeating(
          [](SidePanelCoordinatorAndroid* coordinator, SidePanelEntryKey key,
             tabs::TabInterface* tab) {
            coordinator->SidePanelUIBase::Show(
                key, SidePanelOpenTrigger::kTabChanged,
                /*suppress_animations=*/true);
          },
          coordinator_, first_entry_key));
  auto second_tab_activation_subscription =
      second_tab->RegisterDidActivate(base::BindRepeating(
          [](SidePanelCoordinatorAndroid* coordinator, SidePanelEntryKey key,
             tabs::TabInterface* tab) {
            coordinator->SidePanelUIBase::Show(
                key, SidePanelOpenTrigger::kTabChanged,
                /*suppress_animations=*/true);
          },
          coordinator_, second_entry_key));

  // Arrange: Show the SidePanelEntry for the 2nd tab.
  coordinator_->SidePanelUIBase::Show(second_entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(second_entry_key));

  // Arrange: Switch to the first tab.
  tab_list_->ActivateTab(first_tab->GetHandle());
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));

  // Act: Switch back to second tab.
  tab_list_->ActivateTab(second_tab->GetHandle());
  WaitUntilOpened(coordinator_);

  // Assert: Side panel should show second tab's entry.
  EXPECT_FALSE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));
  EXPECT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(second_entry_key));

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
    MaybeShowEntryOnTabStripModelChanged_SwitchTabs_WindowScopedEntryShowing_NewTabHasNoActiveEntry_KeepsWindowScopedEntry) {
  // Arrange: Open 2 tabs.
  tabs::TabInterface* first_tab = tab_list_->GetActiveTab();
  tabs::TabInterface* second_tab =
      tab_list_->OpenTab(GURL("about:blank"), /*index=*/1);
  ASSERT_FALSE(first_tab->IsActivated());
  ASSERT_TRUE(second_tab->IsActivated());

  // Switch back to first tab to set up window entry while it's active.
  tab_list_->ActivateTab(first_tab->GetHandle());

  // Arrange: Register a window-scoped SidePanelEntry.
  auto* window_registry = SidePanelRegistry::From(browser_);
  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  window_registry->Register(CreateSidePanelEntry(entry_key, browser_));

  // Arrange: Show the window-scoped entry.
  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));

  // Act: Switch to second tab (which has no active entry).
  tab_list_->ActivateTab(second_tab->GetHandle());
  WaitUntilOpened(coordinator_);

  // Assert: Side panel should still show the window-scoped entry.
  EXPECT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    MaybeShowEntryOnTabStripModelChanged_SwitchTabs_WindowScopedEntryShowing_NewTabHasActiveTabScopedEntry_ShowsTabScopedEntry) {
  // Arrange: Open 2 tabs.
  tabs::TabInterface* first_tab = tab_list_->GetActiveTab();
  tabs::TabInterface* second_tab =
      tab_list_->OpenTab(GURL("about:blank"), /*index=*/1);
  ASSERT_FALSE(first_tab->IsActivated());
  ASSERT_TRUE(second_tab->IsActivated());

  // Arrange: Register a window-scoped entry.
  auto* window_registry = SidePanelRegistry::From(browser_);
  auto window_entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto window_entry = CreateSidePanelEntry(window_entry_key, browser_);
  TestSidePanelEntryObserver window_entry_observer(window_entry.get());
  window_registry->Register(std::move(window_entry));

  // Arrange: Register a tab-scoped entry for the 2nd tab.
  auto* second_registry = SidePanelRegistry::From(second_tab);
  auto tab_entry_key = SidePanelEntryKey(SidePanelEntryId::kGlic);
  auto tab_entry = CreateSidePanelEntry(tab_entry_key, browser_);
  TestSidePanelEntryObserver tab_entry_observer(tab_entry.get());
  second_registry->Register(std::move(tab_entry));

  // Activate 1st tab, show window entry.
  tab_list_->ActivateTab(first_tab->GetHandle());
  coordinator_->SidePanelUIBase::Show(window_entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(window_entry_key));

  // Activate 2nd tab
  tab_list_->ActivateTab(second_tab->GetHandle());
  WaitUntilOpened(coordinator_);
  // Initially it continues showing window entry because 2nd tab has no active
  // entry yet.
  ASSERT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(window_entry_key));

  // Show its tab-scoped entry (making it active for 2nd tab).
  coordinator_->SidePanelUIBase::Show(tab_entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(tab_entry_key));
  EXPECT_EQ(SidePanelEntryHideReason::kReplaced,
            window_entry_observer.reason_for_last_entry_will_hide_.value());

  // Reset observer state to cleanly verify the next transition.
  window_entry_observer.reason_for_last_entry_will_hide_.reset();
  tab_entry_observer.reason_for_last_entry_will_hide_.reset();

  // Act: Switch back to 1st tab.
  tab_list_->ActivateTab(first_tab->GetHandle());
  WaitUntilOpened(coordinator_);

  // Assert: Side panel should restore the window-scoped entry.
  EXPECT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(window_entry_key));
  EXPECT_FALSE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(tab_entry_key));

  // Assert: The tab-scoped entry should be notified of "hidden" events due to
  // backgrounding.
  EXPECT_EQ(SidePanelEntryHideReason::kBackgrounded,
            tab_entry_observer.reason_for_last_entry_will_hide_.value());
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    MaybeShowEntryOnTabStripModelChanged_SwitchTabs_TabScopedActive_SwitchToTabAndShowWindowScoped_SwitchBack_RestoresTabScoped) {
  // Arrange: Open 2 tabs.
  tabs::TabInterface* first_tab = tab_list_->GetActiveTab();
  tabs::TabInterface* second_tab =
      tab_list_->OpenTab(GURL("about:blank"), /*index=*/1);
  ASSERT_FALSE(first_tab->IsActivated());
  ASSERT_TRUE(second_tab->IsActivated());

  // Arrange: Register a tab-scoped entry for the 1st tab.
  auto* first_registry = SidePanelRegistry::From(first_tab);
  auto tab_entry_key = SidePanelEntryKey(SidePanelEntryId::kGlic);
  auto tab_entry = CreateSidePanelEntry(tab_entry_key, browser_);
  TestSidePanelEntryObserver tab_entry_observer(tab_entry.get());
  first_registry->Register(std::move(tab_entry));

  // Arrange: Register a window-scoped entry.
  auto* window_registry = SidePanelRegistry::From(browser_);
  auto window_entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto window_entry = CreateSidePanelEntry(window_entry_key, browser_);
  TestSidePanelEntryObserver window_entry_observer(window_entry.get());
  window_registry->Register(std::move(window_entry));

  // Activate 1st tab, show its tab-scoped entry (making it active for 1st tab).
  tab_list_->ActivateTab(first_tab->GetHandle());
  coordinator_->SidePanelUIBase::Show(tab_entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(tab_entry_key));

  // Act: Switch to 2nd tab. Side panel closes because 2nd tab has no active
  // entry.
  tab_list_->ActivateTab(second_tab->GetHandle());
  WaitUntilClosed(coordinator_);
  EXPECT_FALSE(coordinator_->IsSidePanelShowing());
  EXPECT_EQ(SidePanelEntryHideReason::kBackgrounded,
            tab_entry_observer.reason_for_last_entry_will_hide_.value());

  // Reset observer state.
  tab_entry_observer.reason_for_last_entry_will_hide_.reset();

  // Act: Show window-scoped entry on 2nd tab.
  coordinator_->SidePanelUIBase::Show(window_entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(window_entry_key));

  // Act: Switch back to 1st tab.
  tab_list_->ActivateTab(first_tab->GetHandle());
  WaitUntilOpened(coordinator_);

  // Assert: 1st tab's active tab-scoped entry should replace the window-scoped
  // entry.
  EXPECT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(tab_entry_key));
  EXPECT_FALSE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(window_entry_key));
  EXPECT_EQ(SidePanelEntryHideReason::kBackgrounded,
            window_entry_observer.reason_for_last_entry_will_hide_.value());
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    MaybeShowEntryOnTabStripModelChanged_CloseTab_WindowScopedEntryShowing_KeepsWindowScopedEntry) {
  // Arrange: Open 2 tabs.
  tabs::TabInterface* first_tab = tab_list_->GetActiveTab();
  tabs::TabInterface* second_tab =
      tab_list_->OpenTab(GURL("about:blank"), /*index=*/1);
  ASSERT_FALSE(first_tab->IsActivated());
  ASSERT_TRUE(second_tab->IsActivated());

  // Arrange: Register a window-scoped entry.
  auto* window_registry = SidePanelRegistry::From(browser_);
  auto window_entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  window_registry->Register(CreateSidePanelEntry(window_entry_key, browser_));

  // Arrange: Show the window-scoped entry on the 2nd tab.
  coordinator_->SidePanelUIBase::Show(window_entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(window_entry_key));

  // Act: Close the 2nd tab. 1st tab becomes active (has no active entries).
  tab_list_->CloseTab(second_tab->GetHandle());
  WaitUntilOpened(coordinator_);

  // Assert: Side panel should continue showing the window-scoped entry
  // smoothly.
  EXPECT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(window_entry_key));
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    MaybeShowEntryOnTabStripModelChanged_CloseTab_NewActiveTabHasNoEntry_ClosesSidePanel) {
  // Arrange: Open 2 tabs.
  tabs::TabInterface* first_tab = tab_list_->GetActiveTab();
  tabs::TabInterface* second_tab =
      tab_list_->OpenTab(GURL("about:blank"), /*index=*/1);
  ASSERT_FALSE(first_tab->IsActivated());
  ASSERT_TRUE(second_tab->IsActivated());

  // Arrange: Register a SidePanelEntry for the 2nd tab.
  auto* second_registry = SidePanelRegistry::From(second_tab);
  auto second_entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  second_registry->Register(CreateSidePanelEntry(second_entry_key, browser_));

  // Arrange: Show the SidePanelEntry for the 2nd tab.
  coordinator_->SidePanelUIBase::Show(second_entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(second_entry_key));

  // Act: Close second tab.
  tab_list_->CloseTab(second_tab->GetHandle());
  WaitUntilClosed(coordinator_);

  // Assert: Side panel should be closed.
  EXPECT_FALSE(coordinator_->IsSidePanelShowing());
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    MaybeShowEntryOnTabStripModelChanged_CloseTab_NewActiveTabHasActiveEntry_OpensSidePanel) {
  // Arrange: Open the 1st tab and show an entry.
  tabs::TabInterface* first_tab = tab_list_->GetActiveTab();
  auto* first_registry = SidePanelRegistry::From(first_tab);
  auto first_entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  first_registry->Register(CreateSidePanelEntry(first_entry_key, browser_));
  coordinator_->SidePanelUIBase::Show(first_entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));

  // Arrange: Open a 2nd tab.
  // The 1st tab's entry should be closed.
  tabs::TabInterface* second_tab =
      tab_list_->OpenTab(GURL("about:blank"), /*index=*/1);
  WaitUntilClosed(coordinator_);
  ASSERT_FALSE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));

  // Act: Close the 2nd tab.
  tab_list_->CloseTab(second_tab->GetHandle());
  WaitUntilOpened(coordinator_);

  // Assert: The 1st tab's entry should be shown.
  EXPECT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    MaybeShowEntryOnTabStripModelChanged_CloseTab_BothTabsHaveActiveEntries_EnableDeferredViewReplacement_ReplacesContent) {
  // Arrange: Open the 1st tab and show its entry backed by a ThinWebView.
  tabs::TabInterface* first_tab = tab_list_->GetActiveTab();
  auto* first_registry = SidePanelRegistry::From(first_tab);
  auto first_entry_key = SidePanelEntryKey(SidePanelEntryId::kGlic);
  first_registry->Register(CreateSidePanelEntry(first_entry_key, browser_,
                                                /*use_thin_web_view=*/true));
  coordinator_->SidePanelUIBase::Show(first_entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));

  // Arrange: Open a 2nd tab and show its entry.
  tabs::TabInterface* second_tab =
      tab_list_->OpenTab(GURL("about:blank"), /*index=*/1);
  WaitUntilClosed(coordinator_);
  ASSERT_FALSE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));

  auto* second_registry = SidePanelRegistry::From(second_tab);
  auto second_entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  second_registry->Register(CreateSidePanelEntry(second_entry_key, browser_));
  coordinator_->SidePanelUIBase::Show(second_entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(second_entry_key));

  // Act: Close the 2nd tab.
  // This will cause an active tab change and a side panel content View
  // replacement.
  coordinator_->ConfigDeferredViewReplacementForTesting(true);
  tab_list_->CloseTab(second_tab->GetHandle());

  // Assert:
  // Note that since we enabled the deferred View replacement, we should wait
  // until there is no "pending replaced entry".
  WaitUntilOpened(coordinator_);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !coordinator_->HasPendingReplacedEntryForTesting(); }));
  EXPECT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));

  // Clean up:
  coordinator_->ConfigDeferredViewReplacementForTesting(false);
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    MaybeShowEntryOnTabStripModelChanged_NullRegistry_DoesNotCrash) {
  // Arrange

  // Act
  // Simulates a tab change to a tab with no WebContents or TabInterface,
  // which causes GetSidePanelRegistryFromWebContents to return nullptr.
  // This verifies that `MaybeShowEntryOnTabStripModelChanged` handles
  // a null registry gracefully.
  coordinator_->SidePanelUIBase::OnActiveTabChanged(nullptr, nullptr, false);

  // Assert
  // The fact that this doesn't crash is the primary assertion.
  EXPECT_FALSE(coordinator_->IsSidePanelShowing());
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    MaybeShowEntryOnTabStripModelChanged_InsufficientSpace_Blocked) {
  // Arrange: Open 2 tabs, both with their own entries.
  tabs::TabInterface* tab_1 = tab_list_->GetActiveTab();
  tabs::TabInterface* tab_2 =
      tab_list_->OpenTab(GURL("about:blank"), /*index=*/1);

  auto entry_key_1 = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto entry_key_2 = SidePanelEntryKey(SidePanelEntryId::kGlic);
  SidePanelRegistry::From(tab_1)->Register(
      CreateSidePanelEntry(entry_key_1, browser_));
  SidePanelRegistry::From(tab_2)->Register(
      CreateSidePanelEntry(entry_key_2, browser_));

  // Show entry in Tab 1.
  tab_list_->ActivateTab(tab_1->GetHandle());
  coordinator_->SidePanelUIBase::Show(
      entry_key_1, SidePanelOpenTrigger::kToolbarButton, true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(coordinator_->IsSidePanelShowing());

  // Show entry in Tab 2.
  tab_list_->ActivateTab(tab_2->GetHandle());
  WaitUntilClosed(coordinator_);
  coordinator_->SidePanelUIBase::Show(
      entry_key_2, SidePanelOpenTrigger::kToolbarButton, true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(coordinator_->IsSidePanelShowing());

  // Make the space insufficient. This will auto-close the panel.
  coordinator_->OnWillAutoClose();
  WaitUntilClosed(coordinator_);
  ASSERT_FALSE(coordinator_->IsSidePanelShowing());

  // Act: Switch to Tab 1 while the space is still insufficient.
  tab_list_->ActivateTab(tab_1->GetHandle());

  // Assert: Side panel should NOT be shown even though Tab 1 has an entry.
  EXPECT_FALSE(coordinator_->IsSidePanelShowing());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       ShowAndClose_TogglesSidePanel) {
  // Arrange:

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser_);
  TestSidePanelEntryObserver entry_observer(entry.get());

  auto* registry = SidePanelRegistry::From(browser_);
  registry->Register(std::move(entry));

  // Act: Show
  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);

  // Assert:
  EXPECT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));
  EXPECT_EQ(entry_key.id(), entry_observer.id_for_last_entry_shown_.value());

  // Act: Close
  coordinator_->Close(SidePanelEntryHideReason::kSidePanelClosed,
                      /*suppress_animations=*/true);
  WaitUntilClosed(coordinator_);

  // Assert:
  EXPECT_FALSE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));
  EXPECT_EQ(entry_key.id(), entry_observer.id_for_last_entry_hidden_.value());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       Close_CancelsLoadingAndClosesShowingEntry) {
  // Arrange:

  auto* registry = SidePanelRegistry::From(browser_);

  // 1. Show the first entry (AboutThisSite).
  auto first_entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  registry->Register(CreateSidePanelEntry(first_entry_key, browser_));
  coordinator_->SidePanelUIBase::Show(first_entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));

  // 2. Prepare a second entry (Glic) that is not available immediately.
  auto second_entry_key = SidePanelEntryKey(SidePanelEntryId::kGlic);
  auto on_view_created =
      base::BindRepeating([](SidePanelNativeViewAndroid* view) {
        SidePanelUtil::GetSidePanelContentProxy(view)->SetAvailable(false);
      });
  registry->Register(CreateSidePanelEntry(second_entry_key, browser_,
                                          /*use_thin_web_view=*/false,
                                          on_view_created));

  // 3. Act: Start showing the second entry (starts loading).
  coordinator_->SetNoDelaysForTesting(false);
  coordinator_->SidePanelUIBase::Show(second_entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);

  // 4. Assert: First entry is still showing, second is loading.
  EXPECT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));
  EXPECT_FALSE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(second_entry_key));
  EXPECT_EQ(registry->GetEntryForKey(second_entry_key),
            coordinator_->GetWaiterForTesting()->loading_entry());

  // 5. Act: Close the side panel.
  coordinator_->Close(SidePanelEntryHideReason::kSidePanelClosed,
                      /*suppress_animations=*/true);
  WaitUntilClosed(coordinator_);

  // 6. Assert: Everything is closed/cancelled.
  EXPECT_FALSE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));
  EXPECT_FALSE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(second_entry_key));
  EXPECT_EQ(nullptr, coordinator_->GetWaiterForTesting()->loading_entry());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       Toggle_ClosedPanel_OpensPanel) {
  // Arrange:

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto* registry = SidePanelRegistry::From(browser_);
  registry->Register(CreateSidePanelEntry(entry_key, browser_));

  ASSERT_FALSE(coordinator_->IsSidePanelShowing());

  // Act:
  coordinator_->Toggle(entry_key, SidePanelOpenTrigger::kToolbarButton);
  WaitUntilOpened(coordinator_);

  // Assert:
  EXPECT_TRUE(coordinator_->IsSidePanelShowing());
  EXPECT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       Toggle_SameEntry_ClosesPanel) {
  // Arrange:

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto* registry = SidePanelRegistry::From(browser_);
  registry->Register(CreateSidePanelEntry(entry_key, browser_));

  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));

  // Act:
  coordinator_->Toggle(entry_key, SidePanelOpenTrigger::kToolbarButton);
  WaitUntilClosed(coordinator_);

  // Assert:
  EXPECT_FALSE(coordinator_->IsSidePanelShowing());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       Toggle_DifferentEntry_ReplacesContent) {
  // Arrange:

  auto first_entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto second_entry_key = SidePanelEntryKey(SidePanelEntryId::kGlic);
  auto* registry = SidePanelRegistry::From(browser_);
  registry->Register(CreateSidePanelEntry(first_entry_key, browser_));
  registry->Register(CreateSidePanelEntry(second_entry_key, browser_));

  coordinator_->SidePanelUIBase::Show(first_entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));

  // Act:
  coordinator_->Toggle(second_entry_key, SidePanelOpenTrigger::kToolbarButton);
  WaitUntilOpened(coordinator_);

  // Assert:
  EXPECT_TRUE(coordinator_->IsSidePanelShowing());
  EXPECT_FALSE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(first_entry_key));
  EXPECT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(second_entry_key));
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       OnWillAutoRestore_RestoresTabScopedSidePanels) {
  // Arrange
  tabs::TabInterface* tab_1 = tab_list_->GetActiveTab();
  tabs::TabInterface* tab_2 =
      tab_list_->OpenTab(GURL("about:blank"), /*index=*/1);

  auto entry_key_1 = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto entry_key_2 = SidePanelEntryKey(SidePanelEntryId::kGlic);
  SidePanelRegistry::From(tab_1)->Register(
      CreateSidePanelEntry(entry_key_1, browser_));
  SidePanelRegistry::From(tab_2)->Register(
      CreateSidePanelEntry(entry_key_2, browser_));

  // Open Tab 1 and its tab-scoped side panel.
  tab_list_->ActivateTab(tab_1->GetHandle());
  coordinator_->SidePanelUIBase::Show(
      entry_key_1, SidePanelOpenTrigger::kToolbarButton, true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(coordinator_->IsSidePanelShowing());

  // Open Tab 2 and its tab-scoped side panel.
  tab_list_->ActivateTab(tab_2->GetHandle());
  WaitUntilClosed(coordinator_);
  coordinator_->SidePanelUIBase::Show(
      entry_key_2, SidePanelOpenTrigger::kToolbarButton, true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(coordinator_->IsSidePanelShowing());

  // Make the space insufficient.
  coordinator_->OnWillAutoClose();
  WaitUntilClosed(coordinator_);

  // Tab 2's side panel is hidden (here we'll clear the active entry in Tab 2's
  // SidePanelRegistry).
  ASSERT_FALSE(coordinator_->IsSidePanelShowing());
  EXPECT_FALSE(SidePanelRegistry::From(tab_2)->GetActiveEntry().has_value());

  // Go to Tab 1.
  tab_list_->ActivateTab(tab_1->GetHandle());

  // Tab 1's side panel shouldn't appear because the space is still
  // insufficient.
  ASSERT_FALSE(coordinator_->IsSidePanelShowing());

  // Now make the space sufficient.
  coordinator_->OnWillAutoRestore();
  WaitUntilOpened(coordinator_);

  // Tab 1's side panel should appear (note that this is not the side panel
  // entry that was hidden when the window became narrow).
  EXPECT_TRUE(coordinator_->IsSidePanelShowing());
  EXPECT_TRUE(coordinator_->IsSidePanelEntryShowing(entry_key_1));

  // Go to Tab 2.
  tab_list_->ActivateTab(tab_2->GetHandle());
  WaitUntilOpened(coordinator_);

  // Tab 2’s side panel should also appear (note that Tab 2’s SidePanelRegistry
  // had no active entry when we went to Tab 2).
  EXPECT_TRUE(coordinator_->IsSidePanelShowing());
  EXPECT_TRUE(coordinator_->IsSidePanelEntryShowing(entry_key_2));
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       OnWillAutoRestore_RestoresMixedScopedSidePanels) {
  // Arrange
  tabs::TabInterface* tab_1 = tab_list_->GetActiveTab();
  tabs::TabInterface* tab_2 =
      tab_list_->OpenTab(GURL("about:blank"), /*index=*/1);

  auto tab_entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto window_entry_key = SidePanelEntryKey(SidePanelEntryId::kGlic);

  SidePanelRegistry::From(tab_1)->Register(
      CreateSidePanelEntry(tab_entry_key, browser_));
  SidePanelRegistry::From(browser_)->Register(
      CreateSidePanelEntry(window_entry_key, browser_));

  // Open Tab 1 and its tab-scoped side panel.
  tab_list_->ActivateTab(tab_1->GetHandle());
  coordinator_->SidePanelUIBase::Show(
      tab_entry_key, SidePanelOpenTrigger::kToolbarButton, true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(coordinator_->IsSidePanelShowing());

  // Open Tab 2 and a window-scoped side panel.
  tab_list_->ActivateTab(tab_2->GetHandle());
  WaitUntilClosed(coordinator_);
  coordinator_->SidePanelUIBase::Show(
      window_entry_key, SidePanelOpenTrigger::kToolbarButton, true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(coordinator_->IsSidePanelShowing());

  // Go to Tab 1.
  tab_list_->ActivateTab(tab_1->GetHandle());
  WaitUntilOpened(coordinator_);

  // Make the space insufficient.
  coordinator_->OnWillAutoClose();
  WaitUntilClosed(coordinator_);
  ASSERT_FALSE(coordinator_->IsSidePanelShowing());

  // Go to Tab 2.
  tab_list_->ActivateTab(tab_2->GetHandle());
  ASSERT_FALSE(coordinator_->IsSidePanelShowing());

  // Make the space sufficient.
  coordinator_->OnWillAutoRestore();
  WaitUntilOpened(coordinator_);

  // Tab 2’s window-scoped panel should appear.
  EXPECT_TRUE(coordinator_->IsSidePanelShowing());
  EXPECT_TRUE(coordinator_->IsSidePanelEntryShowing(window_entry_key));
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       OnWillAutoClose_ClosesSidePanel) {
  // Arrange:

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser_);
  TestSidePanelEntryObserver entry_observer(entry.get());

  auto* registry = SidePanelRegistry::From(browser_);
  registry->Register(std::move(entry));

  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(
      coordinator_->SidePanelUIBase::IsSidePanelEntryShowing(entry_key));

  // Act:
  coordinator_->OnWillAutoClose();
  WaitUntilClosed(coordinator_);

  // Assert:
  EXPECT_FALSE(coordinator_->IsSidePanelShowing());
  EXPECT_EQ(SidePanelEntryHideReason::kWindowResized,
            entry_observer.reason_for_last_entry_hidden_with_reason_.value());

  // Assert: Registry should be reset (consistent with kBackgrounded).
  EXPECT_FALSE(registry->GetActiveEntry().has_value());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       OnWillAutoRestore_NoActiveEntry_DoesNothing) {
  // Arrange: Make the space insufficient first.
  coordinator_->OnWillAutoClose();
  ASSERT_FALSE(coordinator_->IsSidePanelShowing());

  // Act: Make the space sufficient again.
  coordinator_->OnWillAutoRestore();

  // Assert: Panel should stay closed.
  EXPECT_FALSE(coordinator_->IsSidePanelShowing());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       OnWillAutoRestore_RestoresPreviousEntry) {
  // Arrange:
  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  SidePanelRegistry::From(browser_)->Register(
      CreateSidePanelEntry(entry_key, browser_));

  // Show and then hide due to insufficient space.
  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(coordinator_->IsSidePanelShowing());
  coordinator_->OnWillAutoClose();
  WaitUntilClosed(coordinator_);
  ASSERT_FALSE(coordinator_->IsSidePanelShowing());

  // Act:
  coordinator_->OnWillAutoRestore();
  WaitUntilOpened(coordinator_);

  // Assert:
  EXPECT_TRUE(coordinator_->IsSidePanelShowing());
  EXPECT_TRUE(coordinator_->IsSidePanelEntryShowing(entry_key));
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    OnWillAutoRestore_TabIsolation_DoesNotRestoreOnDifferentTab) {
  // Arrange: Open 2 tabs.
  tabs::TabInterface* tab_with_entry = tab_list_->GetActiveTab();
  tabs::TabInterface* empty_tab =
      tab_list_->OpenTab(GURL("about:blank"), /*index=*/1);

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  SidePanelRegistry::From(tab_with_entry)
      ->Register(CreateSidePanelEntry(entry_key, browser_));

  // Activate the tab that has the entry and show it.
  tab_list_->ActivateTab(tab_with_entry->GetHandle());
  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(coordinator_->IsSidePanelShowing());

  // Hide the panel due to insufficient space.
  coordinator_->OnWillAutoClose();
  WaitUntilClosed(coordinator_);
  ASSERT_FALSE(coordinator_->IsSidePanelShowing());

  // Switch to the empty tab.
  tab_list_->ActivateTab(empty_tab->GetHandle());

  // Act: Try to restore visibility while on the wrong tab.
  coordinator_->OnWillAutoRestore();

  // Assert: The panel should NOT restore on the empty tab.
  EXPECT_FALSE(coordinator_->IsSidePanelShowing());

  // Act: Switch back to the original tab.
  // This should trigger restoration automatically via
  // MaybeShowEntryOnTabStripModelChanged.
  tab_list_->ActivateTab(tab_with_entry->GetHandle());
  WaitUntilOpened(coordinator_);

  // Assert: The panel should now restore correctly on the original tab.
  EXPECT_TRUE(coordinator_->IsSidePanelShowing());
  EXPECT_TRUE(coordinator_->IsSidePanelEntryShowing(entry_key));
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    OnWillAutoRestore_TabIsolation_SameEntryKeyOnMultipleTabs) {
  // Arrange: Open 2 tabs, both with the SAME entry key registered.
  tabs::TabInterface* tab_1 = tab_list_->GetActiveTab();
  tabs::TabInterface* tab_2 =
      tab_list_->OpenTab(GURL("about:blank"), /*index=*/1);

  auto same_entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  SidePanelRegistry::From(tab_1)->Register(
      CreateSidePanelEntry(same_entry_key, browser_));
  SidePanelRegistry::From(tab_2)->Register(
      CreateSidePanelEntry(same_entry_key, browser_));

  // 1. Open entry in Tab 1.
  tab_list_->ActivateTab(tab_1->GetHandle());
  coordinator_->SidePanelUIBase::Show(
      same_entry_key, SidePanelOpenTrigger::kToolbarButton, true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(coordinator_->IsSidePanelShowing());

  // 2. Hide due to insufficient space.
  coordinator_->OnWillAutoClose();
  WaitUntilClosed(coordinator_);
  ASSERT_FALSE(coordinator_->IsSidePanelShowing());

  // 3. Switch to Tab 2.
  tab_list_->ActivateTab(tab_2->GetHandle());

  // 4. Make space sufficient.
  coordinator_->OnWillAutoRestore();

  // Assert: Entry should NOT show in Tab 2 even though it has the same key.
  // This proves that UniqueKey (tab-aware) is used for restoration.
  EXPECT_FALSE(coordinator_->IsSidePanelShowing());

  // 5. Open the same entry key in Tab 2 manually.
  coordinator_->SidePanelUIBase::Show(
      same_entry_key, SidePanelOpenTrigger::kToolbarButton, true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(coordinator_->IsSidePanelShowing());
  ASSERT_TRUE(coordinator_->IsSidePanelEntryShowing(same_entry_key));

  // 6. Switch back to Tab 1.
  tab_list_->ActivateTab(tab_1->GetHandle());
  WaitUntilOpened(coordinator_);

  // Assert: Entry should be restored automatically on Tab 1 because the
  // restoration key was specific to Tab 1 and was never cleared.
  EXPECT_TRUE(coordinator_->IsSidePanelShowing());
  EXPECT_TRUE(coordinator_->IsSidePanelEntryShowing(same_entry_key));
}

// Setup:
// Source window: reparent a tab with a tab-scoped panel
// Source window (post-reparenting): no side panel
// Target window (pre-reparenting): no side panel
IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    ReparentTabWithTabScopedEntry_SrcWindowHasNoEntryPostReparenting_DstWindowHasNoEntryPreReparenting) {
  BrowserWindowInterface* src_window = GetLastActiveBrowser();
  SetUpSidePanelEntriesForWindow(
      src_window, /*window_scoped_entry_id=*/std::nullopt,
      {std::nullopt, SidePanelEntryId::kGlic}, /*active_tab_index=*/1);
  auto* src_coordinator = SidePanelCoordinatorAndroid::From(src_window);
  auto* src_tab_list = TabListInterface::From(src_window);

  BrowserWindowInterface* dst_window =
      CreateBrowserWindowAsync(src_window->GetProfile());
  SetUpSidePanelEntriesForWindow(dst_window,
                                 /*window_scoped_entry_id=*/std::nullopt,
                                 {std::nullopt}, /*active_tab_index=*/0);
  auto* dst_coordinator = SidePanelCoordinatorAndroid::From(dst_window);

  src_tab_list->MoveTabToWindow(src_tab_list->GetTab(1)->GetHandle(),
                                dst_window->GetSessionID(),
                                /*destination_index=*/0);
  WaitUntilClosed(src_coordinator);
  WaitUntilOpened(dst_coordinator);

  EXPECT_FALSE(src_coordinator->IsSidePanelShowing());
  EXPECT_TRUE(dst_coordinator->IsSidePanelShowing());
  EXPECT_TRUE(dst_coordinator->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kGlic)));
}

// Setup:
// Source window: reparent the sole tab with a tab-scoped panel
// Source window (post-reparenting): closed/destroyed
// Target window (pre-reparenting): no side panel
IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    ReparentTabWithTabScopedEntry_SrcWindowClosedPostReparenting) {
  BrowserWindowInterface* src_window = GetLastActiveBrowser();
  SetUpSidePanelEntriesForWindow(
      src_window, /*window_scoped_entry_id=*/std::nullopt,
      {SidePanelEntryId::kGlic}, /*active_tab_index=*/0);
  auto* src_tab_list = TabListInterface::From(src_window);

  BrowserWindowInterface* dst_window =
      CreateBrowserWindowAsync(src_window->GetProfile());
  SetUpSidePanelEntriesForWindow(dst_window,
                                 /*window_scoped_entry_id=*/std::nullopt,
                                 {std::nullopt}, /*active_tab_index=*/0);
  auto* dst_coordinator = SidePanelCoordinatorAndroid::From(dst_window);

  // Moving the sole tab of src_window to dst_window will trigger src_window to
  // close.
  src_tab_list->MoveTabToWindow(src_tab_list->GetTab(0)->GetHandle(),
                                dst_window->GetSessionID(),
                                /*destination_index=*/0);
  WaitUntilOpened(dst_coordinator);

  // Assert: Target window receives the tab and shows kGlic side panel
  // successfully.
  EXPECT_TRUE(dst_coordinator->IsSidePanelShowing());
  EXPECT_TRUE(dst_coordinator->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kGlic)));
}

// Setup:
// Source window: reparent the sole tab with a window-scoped panel
// Source window (post-reparenting): closed/destroyed
// Target window (pre-reparenting): no side panel
IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    ReparentTabWithWindowScopedEntry_SrcWindowClosedPostReparenting) {
  BrowserWindowInterface* src_window = GetLastActiveBrowser();
  SetUpSidePanelEntriesForWindow(
      src_window, /*window_scoped_entry_id=*/SidePanelEntryId::kBookmarks,
      {std::nullopt}, /*active_tab_index=*/0);
  auto* src_tab_list = TabListInterface::From(src_window);

  BrowserWindowInterface* dst_window =
      CreateBrowserWindowAsync(src_window->GetProfile());
  SetUpSidePanelEntriesForWindow(dst_window,
                                 /*window_scoped_entry_id=*/std::nullopt,
                                 {std::nullopt}, /*active_tab_index=*/0);
  auto* dst_coordinator = SidePanelCoordinatorAndroid::From(dst_window);

  // Moving the sole tab of src_window to dst_window will trigger src_window to
  // close.
  src_tab_list->MoveTabToWindow(src_tab_list->GetTab(0)->GetHandle(),
                                dst_window->GetSessionID(),
                                /*destination_index=*/0);
  // TODO(crbug.com/513302000): Uncomment this once the Java reparenting
  // lifecycle bug is fixed in the upcoming Java refactor CL. Currently, the
  // side panel fails to close physically on the destination window.
  // WaitUntilClosed(dst_coordinator);

  // Assert: Window-scoped side panel entries do not transfer across windows on
  // reparenting.
  EXPECT_FALSE(dst_coordinator->IsSidePanelShowing());
}

// Setup:
// Source window: reparent a tab with a tab-scoped panel
// Source window (post-reparenting): a tab with a tab-scoped panel is active
// Target window (pre-reparenting): no side panel
IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    ReparentTabWithTabScopedEntry_SrcWindowHasTabScopedEntryPostReparenting_DstWindowHasNoEntryPreReparenting) {
  BrowserWindowInterface* src_window = GetLastActiveBrowser();
  SetUpSidePanelEntriesForWindow(
      src_window, /*window_scoped_entry_id=*/std::nullopt,
      {SidePanelEntryId::kAboutThisSite, SidePanelEntryId::kGlic},
      /*active_tab_index=*/1, /*use_thin_web_view=*/true);
  auto* src_coordinator = SidePanelCoordinatorAndroid::From(src_window);
  auto* src_tab_list = TabListInterface::From(src_window);

  BrowserWindowInterface* dst_window =
      CreateBrowserWindowAsync(src_window->GetProfile());
  SetUpSidePanelEntriesForWindow(dst_window,
                                 /*window_scoped_entry_id=*/std::nullopt,
                                 {std::nullopt}, /*active_tab_index=*/0);
  auto* dst_coordinator = SidePanelCoordinatorAndroid::From(dst_window);

  SequenceEnforcingTabModelObserver observer;

  auto* src_tab_model = static_cast<TabModel*>(src_tab_list);
  src_tab_model->AddObserver(&observer);

  src_tab_list->MoveTabToWindow(src_tab_list->GetTab(1)->GetHandle(),
                                dst_window->GetSessionID(),
                                /*destination_index=*/0);

  src_tab_model->RemoveObserver(&observer);

  // Ensure that the TabModel actually fired the expected removal callback
  // during reparenting, which validates the sequence tested above.
  EXPECT_TRUE(observer.tab_removed_called_);

  // This check enforces two critical requirements:
  // 1. Reparenting a tab synchronously flushes any async view detachments
  //    (preventing the source window from maliciously freezing the
  //    reparented tab's WebContents).
  // 2. The `TabModel` must fire the active tab change (`DidSelectTab`)
  //    *before* firing tab removal (`TabRemoved`). If this sequence is ever
  //    inverted by future TabModel changes, this test will fail because
  //    `TabRemoved` would skip the flush (as the replacement hasn't started),
  //    and the replacement would be left pending here.
  EXPECT_FALSE(src_coordinator->HasPendingReplacedEntryForTesting());
  WaitUntilOpened(src_coordinator);
  WaitUntilOpened(dst_coordinator);

  EXPECT_TRUE(src_coordinator->IsSidePanelShowing());
  EXPECT_TRUE(src_coordinator->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kAboutThisSite)));
  EXPECT_TRUE(dst_coordinator->IsSidePanelShowing());
  EXPECT_TRUE(dst_coordinator->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kGlic)));
}

// Setup:
// Source window: reparent a tab under a window-scoped panel
// Source window (post-reparenting): a tab under a window-scoped panel is active
// Target window (pre-reparenting): no side panel
IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    ReparentTabWithWindowScopedEntry_SrcWindowHasWindowScopedEntryPostReparenting_DstWindowHasNoEntryPreReparenting) {
  BrowserWindowInterface* src_window = GetLastActiveBrowser();
  SetUpSidePanelEntriesForWindow(
      src_window, /*window_scoped_entry_id=*/SidePanelEntryId::kBookmarks,
      {std::nullopt, std::nullopt}, /*active_tab_index=*/1);
  auto* src_coordinator = SidePanelCoordinatorAndroid::From(src_window);
  auto* src_tab_list = TabListInterface::From(src_window);

  BrowserWindowInterface* dst_window =
      CreateBrowserWindowAsync(src_window->GetProfile());
  SetUpSidePanelEntriesForWindow(dst_window,
                                 /*window_scoped_entry_id=*/std::nullopt,
                                 {std::nullopt}, /*active_tab_index=*/0);
  auto* dst_coordinator = SidePanelCoordinatorAndroid::From(dst_window);

  src_tab_list->MoveTabToWindow(src_tab_list->GetTab(1)->GetHandle(),
                                dst_window->GetSessionID(),
                                /*destination_index=*/0);
  // TODO(crbug.com/513302000): Uncomment these once the Java reparenting
  // lifecycle bug is fixed in the upcoming Java refactor CL.
  // WaitUntilOpened(src_coordinator);
  // WaitUntilClosed(dst_coordinator);

  EXPECT_TRUE(src_coordinator->IsSidePanelShowing());
  EXPECT_TRUE(src_coordinator->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kBookmarks)));
  EXPECT_FALSE(dst_coordinator->IsSidePanelShowing());
}

// Setup:
// Source window: reparent a tab with a tab-scoped panel
// Source window (post-reparenting): a tab with a tab-scoped panel is active
// Target window (pre-reparenting): a tab with a tab-scoped panel is active
IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    ReparentTabWithTabScopedEntry_SrcWindowHasTabScopedEntryPostReparenting_DstWindowHasTabScopedEntryPreReparenting) {
  BrowserWindowInterface* src_window = GetLastActiveBrowser();
  SetUpSidePanelEntriesForWindow(
      src_window, /*window_scoped_entry_id=*/std::nullopt,
      {SidePanelEntryId::kAboutThisSite, SidePanelEntryId::kGlic},
      /*active_tab_index=*/1);
  auto* src_coordinator = SidePanelCoordinatorAndroid::From(src_window);
  auto* src_tab_list = TabListInterface::From(src_window);

  BrowserWindowInterface* dst_window =
      CreateBrowserWindowAsync(src_window->GetProfile());
  SetUpSidePanelEntriesForWindow(
      dst_window, /*window_scoped_entry_id=*/std::nullopt,
      {SidePanelEntryId::kComments}, /*active_tab_index=*/0);
  auto* dst_coordinator = SidePanelCoordinatorAndroid::From(dst_window);

  src_tab_list->MoveTabToWindow(src_tab_list->GetTab(1)->GetHandle(),
                                dst_window->GetSessionID(),
                                /*destination_index=*/0);
  WaitUntilOpened(src_coordinator);
  WaitUntilOpened(dst_coordinator);

  EXPECT_TRUE(src_coordinator->IsSidePanelShowing());
  EXPECT_TRUE(src_coordinator->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kAboutThisSite)));
  EXPECT_TRUE(dst_coordinator->IsSidePanelShowing());
  EXPECT_TRUE(dst_coordinator->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kGlic)));
}

// Setup:
// Source window: reparent a tab with a tab-scoped panel
// Source window (post-reparenting): a tab with a tab-scoped panel is active
// Target window (pre-reparenting): a tab under a window-scoped panel is active
IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    ReparentTabWithTabScopedEntry_SrcWindowHasTabScopedEntryPostReparenting_DstWindowHasWindowScopedEntryPreReparenting) {
  BrowserWindowInterface* src_window = GetLastActiveBrowser();
  SetUpSidePanelEntriesForWindow(
      src_window, /*window_scoped_entry_id=*/std::nullopt,
      {SidePanelEntryId::kAboutThisSite, SidePanelEntryId::kGlic},
      /*active_tab_index=*/1);
  auto* src_coordinator = SidePanelCoordinatorAndroid::From(src_window);
  auto* src_tab_list = TabListInterface::From(src_window);

  BrowserWindowInterface* dst_window =
      CreateBrowserWindowAsync(src_window->GetProfile());
  SetUpSidePanelEntriesForWindow(
      dst_window, /*window_scoped_entry_id=*/SidePanelEntryId::kReadingList,
      {std::nullopt}, /*active_tab_index=*/0);
  auto* dst_coordinator = SidePanelCoordinatorAndroid::From(dst_window);

  src_tab_list->MoveTabToWindow(src_tab_list->GetTab(1)->GetHandle(),
                                dst_window->GetSessionID(),
                                /*destination_index=*/0);
  // TODO(crbug.com/513302000): Uncomment these once the Java reparenting
  // lifecycle bug is fixed in the upcoming Java refactor CL.
  // WaitUntilOpened(src_coordinator);
  // WaitUntilOpened(dst_coordinator);

  EXPECT_TRUE(src_coordinator->IsSidePanelShowing());
  EXPECT_TRUE(src_coordinator->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kAboutThisSite)));
  EXPECT_TRUE(dst_coordinator->IsSidePanelShowing());
  EXPECT_TRUE(dst_coordinator->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kGlic)));
}

// Setup:
// Source window: reparent a tab with a tab-scoped panel
// Source window (post-reparenting): no side panel
// Target window (pre-reparenting): a tab with a tab-scoped panel is active
IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    ReparentTabWithTabScopedEntry_SrcWindowHasNoEntryPostReparenting_DstWindowHasTabScopedEntryPreReparenting) {
  BrowserWindowInterface* src_window = GetLastActiveBrowser();
  SetUpSidePanelEntriesForWindow(
      src_window, /*window_scoped_entry_id=*/std::nullopt,
      {std::nullopt, SidePanelEntryId::kGlic}, /*active_tab_index=*/1);
  auto* src_coordinator = SidePanelCoordinatorAndroid::From(src_window);
  auto* src_tab_list = TabListInterface::From(src_window);

  BrowserWindowInterface* dst_window =
      CreateBrowserWindowAsync(src_window->GetProfile());
  SetUpSidePanelEntriesForWindow(
      dst_window, /*window_scoped_entry_id=*/std::nullopt,
      {SidePanelEntryId::kComments}, /*active_tab_index=*/0);
  auto* dst_coordinator = SidePanelCoordinatorAndroid::From(dst_window);

  src_tab_list->MoveTabToWindow(src_tab_list->GetTab(1)->GetHandle(),
                                dst_window->GetSessionID(),
                                /*destination_index=*/0);
  WaitUntilClosed(src_coordinator);
  WaitUntilOpened(dst_coordinator);

  EXPECT_FALSE(src_coordinator->IsSidePanelShowing());
  EXPECT_TRUE(dst_coordinator->IsSidePanelShowing());
  EXPECT_TRUE(dst_coordinator->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kGlic)));
}

// Setup:
// Source window: reparent a tab with a tab-scoped panel
// Source window (post-reparenting): no side panel
// Target window (pre-reparenting): a tab under a window-scoped panel is active
IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    ReparentTabWithTabScopedEntry_SrcWindowHasNoEntryPostReparenting_DstWindowHasWindowScopedEntryPreReparenting) {
  BrowserWindowInterface* src_window = GetLastActiveBrowser();
  SetUpSidePanelEntriesForWindow(
      src_window, /*window_scoped_entry_id=*/std::nullopt,
      {std::nullopt, SidePanelEntryId::kGlic}, /*active_tab_index=*/1);
  auto* src_coordinator = SidePanelCoordinatorAndroid::From(src_window);
  auto* src_tab_list = TabListInterface::From(src_window);

  BrowserWindowInterface* dst_window =
      CreateBrowserWindowAsync(src_window->GetProfile());
  SetUpSidePanelEntriesForWindow(
      dst_window, /*window_scoped_entry_id=*/SidePanelEntryId::kReadingList,
      {std::nullopt}, /*active_tab_index=*/0);
  auto* dst_coordinator = SidePanelCoordinatorAndroid::From(dst_window);

  src_tab_list->MoveTabToWindow(src_tab_list->GetTab(1)->GetHandle(),
                                dst_window->GetSessionID(),
                                /*destination_index=*/0);

  // TODO(crbug.com/513302000): Uncomment these once the Java reparenting
  // lifecycle bug is fixed in the upcoming Java refactor CL.
  // WaitUntilClosed(src_coordinator);
  // WaitUntilOpened(dst_coordinator);

  EXPECT_FALSE(src_coordinator->IsSidePanelShowing());
  EXPECT_TRUE(dst_coordinator->IsSidePanelShowing());
  EXPECT_TRUE(dst_coordinator->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kGlic)));
}

// Setup:
// Source window: reparent a tab under a window-scoped panel
// Source window (post-reparenting): a tab under a window-scoped panel is active
// Target window (pre-reparenting): a tab with a tab-scoped panel is active
IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    ReparentTabWithWindowScopedEntry_SrcWindowHasWindowScopedEntryPostReparenting_DstWindowHasTabScopedEntryPreReparenting) {
  BrowserWindowInterface* src_window = GetLastActiveBrowser();
  SetUpSidePanelEntriesForWindow(
      src_window, /*window_scoped_entry_id=*/SidePanelEntryId::kBookmarks,
      {std::nullopt, std::nullopt}, /*active_tab_index=*/1);
  auto* src_coordinator = SidePanelCoordinatorAndroid::From(src_window);
  auto* src_tab_list = TabListInterface::From(src_window);

  BrowserWindowInterface* dst_window =
      CreateBrowserWindowAsync(src_window->GetProfile());
  SetUpSidePanelEntriesForWindow(
      dst_window, /*window_scoped_entry_id=*/std::nullopt,
      {SidePanelEntryId::kComments}, /*active_tab_index=*/0);
  auto* dst_coordinator = SidePanelCoordinatorAndroid::From(dst_window);

  src_tab_list->MoveTabToWindow(src_tab_list->GetTab(1)->GetHandle(),
                                dst_window->GetSessionID(),
                                /*destination_index=*/0);

  // TODO(crbug.com/513302000): Uncomment these once the Java reparenting
  // lifecycle bug is fixed in the upcoming Java refactor CL.
  // WaitUntilOpened(src_coordinator);
  // WaitUntilClosed(dst_coordinator);

  EXPECT_TRUE(src_coordinator->IsSidePanelShowing());
  EXPECT_TRUE(src_coordinator->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kBookmarks)));
  EXPECT_FALSE(dst_coordinator->IsSidePanelShowing());
}

// Setup:
// Source window: reparent a tab under a window-scoped panel
// Source window (post-reparenting): a tab under a window-scoped panel is active
// Target window (pre-reparenting): a tab with a window-scoped panel is active
IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    ReparentTabWithWindowScopedEntry_SrcWindowHasWindowScopedEntryPostReparenting_DstWindowHasWindowScopedEntryPreReparenting) {
  BrowserWindowInterface* src_window = GetLastActiveBrowser();
  SetUpSidePanelEntriesForWindow(
      src_window, /*window_scoped_entry_id=*/SidePanelEntryId::kBookmarks,
      {std::nullopt, std::nullopt}, /*active_tab_index=*/1);
  auto* src_coordinator = SidePanelCoordinatorAndroid::From(src_window);
  auto* src_tab_list = TabListInterface::From(src_window);

  BrowserWindowInterface* dst_window =
      CreateBrowserWindowAsync(src_window->GetProfile());
  SetUpSidePanelEntriesForWindow(
      dst_window, /*window_scoped_entry_id=*/SidePanelEntryId::kReadingList,
      {std::nullopt}, /*active_tab_index=*/0);
  auto* dst_coordinator = SidePanelCoordinatorAndroid::From(dst_window);

  src_tab_list->MoveTabToWindow(src_tab_list->GetTab(1)->GetHandle(),
                                dst_window->GetSessionID(),
                                /*destination_index=*/0);

  // TODO(crbug.com/513302000): Uncomment these once the Java reparenting
  // lifecycle bug is fixed in the upcoming Java refactor CL.
  // WaitUntilOpened(src_coordinator);
  // WaitUntilOpened(dst_coordinator);

  // Assert: Source window retains its Bookmarks window-scoped entry.
  EXPECT_TRUE(src_coordinator->IsSidePanelShowing());
  EXPECT_TRUE(src_coordinator->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kBookmarks)));

  // Assert: Target window retains its Reading List window-scoped entry.
  EXPECT_TRUE(dst_coordinator->IsSidePanelShowing());
  EXPECT_TRUE(dst_coordinator->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kReadingList)));
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       TabClosure_ClearsDeferredEntryTracker) {
  // Arrange
  tabs::TabInterface* tab_1 = tab_list_->GetActiveTab();

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  SidePanelRegistry::From(tab_1)->Register(
      CreateSidePanelEntry(entry_key, browser_));

  // Open Tab 1 and its tab-scoped panel.
  tab_list_->ActivateTab(tab_1->GetHandle());
  coordinator_->SidePanelUIBase::Show(
      entry_key, SidePanelOpenTrigger::kToolbarButton, true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(coordinator_->IsSidePanelShowing());

  // Make the space insufficient. This defers the entry.
  coordinator_->OnWillAutoClose();
  WaitUntilClosed(coordinator_);
  ASSERT_FALSE(coordinator_->IsSidePanelShowing());

  const SidePanelDeferredEntryTracker& tracker =
      coordinator_->GetDeferredEntryTrackerForTesting();
  tabs::TabHandle tab_1_handle = tab_1->GetHandle();
  ASSERT_TRUE(tracker.GetEntry(tab_1_handle).has_value());

  // Open Tab 2.
  tabs::TabInterface* tab_2 =
      tab_list_->OpenTab(GURL("about:blank"), /*index=*/1);
  tab_list_->ActivateTab(tab_2->GetHandle());

  // Act: Close Tab 1.
  tab_list_->CloseTab(tab_1_handle);

  // Assert: The tracker should no longer hold an entry for Tab 1.
  EXPECT_FALSE(tracker.GetEntry(tab_1_handle).has_value());
}
IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       TabSwitch_DefersEntry_ClearsRegistry) {
  // Arrange
  tabs::TabInterface* tab_1 = tab_list_->GetActiveTab();
  tabs::TabInterface* tab_2 =
      tab_list_->OpenTab(GURL("about:blank"), /*index=*/1);

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  SidePanelRegistry::From(tab_1)->Register(
      CreateSidePanelEntry(entry_key, browser_));

  // 1. Open side panel on Tab A.
  tab_list_->ActivateTab(tab_1->GetHandle());
  coordinator_->SidePanelUIBase::Show(
      entry_key, SidePanelOpenTrigger::kToolbarButton, true);
  WaitUntilOpened(coordinator_);
  ASSERT_TRUE(coordinator_->IsSidePanelShowing());
  EXPECT_TRUE(SidePanelRegistry::From(tab_1)->GetActiveEntry().has_value());

  // 2. Switch to Tab B.
  tab_list_->ActivateTab(tab_2->GetHandle());
  WaitUntilClosed(coordinator_);
  ASSERT_FALSE(coordinator_->IsSidePanelShowing());
  // CRITICAL: The entry remains active in Tab 1's background registry!
  EXPECT_TRUE(SidePanelRegistry::From(tab_1)->GetActiveEntry().has_value());

  // 3. Make the space insufficient.
  coordinator_->OnWillAutoClose();

  // 4. Switch back to Tab A.
  // This triggers MaybeShowEntryOnTabStripModelChanged -> Show() -> AddEntry().
  tab_list_->ActivateTab(tab_1->GetHandle());

  // Assert: The panel cannot show because the window is small.
  EXPECT_FALSE(coordinator_->IsSidePanelShowing());

  // Assert: The entry should have been successfully deferred into the tracker.
  const SidePanelDeferredEntryTracker& tracker =
      coordinator_->GetDeferredEntryTrackerForTesting();
  EXPECT_TRUE(tracker.GetEntry(tab_1->GetHandle()).has_value());

  // Because the entry is now deferred, it should not be active in the registry
  // anymore. If AddEntry() didn't have ResetActiveEntry(), this expectation
  // would fail.
  EXPECT_FALSE(SidePanelRegistry::From(tab_1)->GetActiveEntry().has_value());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       Init_RestoresActiveEntryFromActiveTab) {
  // Arrange: Get the active tab and its registry.
  tabs::TabInterface* tab = tab_list_->GetActiveTab();
  auto* registry = SidePanelRegistry::From(tab);

  // Register a test entry.
  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser_);
  SidePanelEntry* entry_ptr = entry.get();
  registry->Register(std::move(entry));

  // Simulate the tab tear-off case:
  // (1) Make the entry active in the registry without calling
  // coordinator_->Show()
  registry->SetActiveEntry(entry_ptr);

  // (2) The coordinator won't be aware of the active entry since we didn't call
  // Show().
  EXPECT_FALSE(coordinator_->IsSidePanelShowing());

  // Act: Call Init.
  coordinator_->Init();
  WaitUntilOpened(coordinator_);

  // Assert: The coordinator should now be showing the side panel with the
  // correct entry.
  EXPECT_TRUE(coordinator_->IsSidePanelShowing());
  EXPECT_TRUE(coordinator_->IsSidePanelEntryShowing(entry_key));
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       HasContentToShow_InitialState_ReturnsFalse) {
  EXPECT_FALSE(coordinator_->HasContentToShow());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       HasContentToShow_AfterSidePanelIsShown_ReturnsTrue) {
  // Arrange:
  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto* registry = SidePanelRegistry::From(browser_);
  registry->Register(CreateSidePanelEntry(entry_key, browser_));

  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);

  // Assert:
  EXPECT_TRUE(coordinator_->HasContentToShow());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       HasContentToShow_AfterSidePanelIsClosed_ReturnsFalse) {
  // Arrange:
  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto* registry = SidePanelRegistry::From(browser_);
  registry->Register(CreateSidePanelEntry(entry_key, browser_));

  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);

  coordinator_->Close(SidePanelEntryHideReason::kSidePanelClosed,
                      /*suppress_animations=*/true);
  WaitUntilClosed(coordinator_);

  // Assert:
  EXPECT_FALSE(coordinator_->HasContentToShow());
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    HasContentToShow_AfterSidePanelIsClosedWithDeferredEntry_ReturnsTrue) {
  // Arrange: Show a side panel entry.
  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto* registry = SidePanelRegistry::From(browser_);
  registry->Register(CreateSidePanelEntry(entry_key, browser_));

  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);

  // Arrange: Make the space insufficient, which will create a deferred entry
  // and close the side panel.
  coordinator_->OnWillAutoClose();
  WaitUntilClosed(coordinator_);

  // Assert:
  EXPECT_TRUE(coordinator_->HasContentToShow());
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    TestAnimations_ShowThenClose_WaitForAnimationsToEnd_OnEntryShownAndOnEntryHiddenCalled) {
  // Arrange:
  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser_);
  TestSidePanelEntryObserver entry_observer(entry.get());

  auto* registry = SidePanelRegistry::From(browser_);
  registry->Register(std::move(entry));

  // Act: Open the side panel, wait for animations to end, then close it.
  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/false);
  WaitUntilOpened(coordinator_);
  coordinator_->Close(SidePanelEntryHideReason::kSidePanelClosed,
                      /*suppress_animations=*/false);
  WaitUntilClosed(coordinator_);

  // Assert:
  EXPECT_EQ(entry_key.id(), entry_observer.id_for_last_entry_shown_.value());
  EXPECT_EQ(entry_key.id(), entry_observer.id_for_last_entry_hidden_.value());
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    TestAnimations_ShowThenClose_DoNotWaitForAnimations_OnEntryShownAndOnEntryHiddenCalled) {
  // Arrange:
  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser_);
  TestSidePanelEntryObserver entry_observer(entry.get());

  auto* registry = SidePanelRegistry::From(browser_);
  registry->Register(std::move(entry));

  // Act: Open the side panel, then immediately close it.
  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/false);
  coordinator_->Close(SidePanelEntryHideReason::kSidePanelClosed,
                      /*suppress_animations=*/false);
  WaitUntilClosed(coordinator_);

  // Assert:
  EXPECT_EQ(entry_key.id(), entry_observer.id_for_last_entry_shown_.value());
  EXPECT_EQ(entry_key.id(), entry_observer.id_for_last_entry_hidden_.value());
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    TestAnimations_CloseThenShow_DifferentEntries_WaitForAnimationsToEnd_OnEntryHiddenAndOnEntryShownCalled) {
  // Arrange:
  auto entry_1_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto entry_2_key = SidePanelEntryKey(SidePanelEntryId::kBookmarks);
  std::unique_ptr<SidePanelEntry> entry_1 =
      CreateSidePanelEntry(entry_1_key, browser_);
  std::unique_ptr<SidePanelEntry> entry_2 =
      CreateSidePanelEntry(entry_2_key, browser_);
  TestSidePanelEntryObserver entry_1_observer(entry_1.get());
  TestSidePanelEntryObserver entry_2_observer(entry_2.get());

  auto* registry = SidePanelRegistry::From(browser_);
  registry->Register(std::move(entry_1));
  registry->Register(std::move(entry_2));

  coordinator_->SidePanelUIBase::Show(entry_1_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);

  // Act: Close the side panel, wait for animations to end, then open it.
  coordinator_->Close(SidePanelEntryHideReason::kSidePanelClosed,
                      /*suppress_animations=*/false);
  WaitUntilClosed(coordinator_);
  coordinator_->SidePanelUIBase::Show(entry_2_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/false);
  WaitUntilOpened(coordinator_);

  // Assert:
  EXPECT_EQ(entry_1_key.id(),
            entry_1_observer.id_for_last_entry_shown_.value());
  EXPECT_EQ(entry_1_key.id(),
            entry_1_observer.id_for_last_entry_hidden_.value());
  EXPECT_EQ(entry_2_key.id(),
            entry_2_observer.id_for_last_entry_shown_.value());
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    TestAnimations_CloseThenShow_DifferentEntries_DoNotWaitForAnimations_OnEntryHiddenAndOnEntryShownCalled) {
  // Arrange:
  auto entry_1_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto entry_2_key = SidePanelEntryKey(SidePanelEntryId::kBookmarks);
  std::unique_ptr<SidePanelEntry> entry_1 =
      CreateSidePanelEntry(entry_1_key, browser_);
  std::unique_ptr<SidePanelEntry> entry_2 =
      CreateSidePanelEntry(entry_2_key, browser_);
  TestSidePanelEntryObserver entry_1_observer(entry_1.get());
  TestSidePanelEntryObserver entry_2_observer(entry_2.get());

  auto* registry = SidePanelRegistry::From(browser_);
  registry->Register(std::move(entry_1));
  registry->Register(std::move(entry_2));

  coordinator_->SidePanelUIBase::Show(entry_1_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);

  // Act: Close the side panel, then immediately open it.
  coordinator_->Close(SidePanelEntryHideReason::kSidePanelClosed,
                      /*suppress_animations=*/false);
  coordinator_->SidePanelUIBase::Show(entry_2_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/false);
  WaitUntilOpened(coordinator_);

  // Assert:
  EXPECT_EQ(entry_1_key.id(),
            entry_1_observer.id_for_last_entry_shown_.value());
  EXPECT_EQ(entry_1_key.id(),
            entry_1_observer.id_for_last_entry_hidden_.value());
  EXPECT_EQ(entry_2_key.id(),
            entry_2_observer.id_for_last_entry_shown_.value());
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    TestAnimations_CloseThenShow_SameEntry_WaitForAnimationsToEnd_OnEntryHiddenAndOnEntryShownCalled) {
  // Arrange:
  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser_);
  TestSidePanelEntryObserver entry_observer(entry.get());

  auto* registry = SidePanelRegistry::From(browser_);
  registry->Register(std::move(entry));

  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);

  // Act: Close the side panel, wait for animations to end, then open it.
  coordinator_->Close(SidePanelEntryHideReason::kSidePanelClosed,
                      /*suppress_animations=*/false);
  WaitUntilClosed(coordinator_);
  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/false);
  WaitUntilOpened(coordinator_);

  // Assert:
  EXPECT_EQ(entry_key.id(), entry_observer.id_for_last_entry_shown_.value());
  EXPECT_EQ(entry_key.id(), entry_observer.id_for_last_entry_hidden_.value());
  EXPECT_EQ(2, entry_observer.num_on_entry_shown_received_);
  EXPECT_EQ(1, entry_observer.num_on_entry_hidden_received_);
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    TestAnimations_CloseThenShow_SameEntry_DoNotWaitForAnimations_OnEntryHiddenAndOnEntryShownCalled) {
  // Arrange:
  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser_);
  TestSidePanelEntryObserver entry_observer(entry.get());

  auto* registry = SidePanelRegistry::From(browser_);
  registry->Register(std::move(entry));

  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);

  // Act: Close the side panel, then immediately open it.
  coordinator_->Close(SidePanelEntryHideReason::kSidePanelClosed,
                      /*suppress_animations=*/false);
  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/false);
  WaitUntilOpened(coordinator_);

  // Assert:
  EXPECT_EQ(entry_key.id(), entry_observer.id_for_last_entry_shown_.value());
  EXPECT_EQ(2, entry_observer.num_on_entry_shown_received_);
  EXPECT_EQ(entry_key.id(),
            entry_observer.id_for_last_entry_hide_cancelled_.value());
  EXPECT_EQ(1, entry_observer.num_on_entry_hide_cancelled_received_);
  EXPECT_EQ(0, entry_observer.num_on_entry_hidden_received_);
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    TestAnimations_ShowThenShow_DifferentEntries_WaitForAnimationsToEnd_ReplacesSidePanelContent) {
  // Arrange:
  auto entry_1_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto entry_2_key = SidePanelEntryKey(SidePanelEntryId::kBookmarks);
  std::unique_ptr<SidePanelEntry> entry_1 =
      CreateSidePanelEntry(entry_1_key, browser_);
  std::unique_ptr<SidePanelEntry> entry_2 =
      CreateSidePanelEntry(entry_2_key, browser_);
  TestSidePanelEntryObserver entry_1_observer(entry_1.get());
  TestSidePanelEntryObserver entry_2_observer(entry_2.get());

  auto* registry = SidePanelRegistry::From(browser_);
  registry->Register(std::move(entry_1));
  registry->Register(std::move(entry_2));

  // Act: Open the side panel, wait for animations to end, then show a different
  // entry.
  coordinator_->SidePanelUIBase::Show(entry_1_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/false);
  WaitUntilOpened(coordinator_);
  coordinator_->SidePanelUIBase::Show(entry_2_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/false);
  WaitUntilOpened(coordinator_);

  // Assert:
  EXPECT_EQ(entry_1_key.id(),
            entry_1_observer.id_for_last_entry_shown_.value());
  EXPECT_EQ(entry_1_key.id(),
            entry_1_observer.id_for_last_entry_hidden_.value());
  EXPECT_EQ(SidePanelEntryHideReason::kReplaced,
            entry_1_observer.reason_for_last_entry_hidden_with_reason_.value());
  EXPECT_EQ(entry_2_key.id(),
            entry_2_observer.id_for_last_entry_shown_.value());
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    TestAnimations_ShowThenShow_DifferentEntries_DoNotWaitForAnimations_ReplacesSidePanelContent) {
  // Arrange:
  auto entry_1_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  auto entry_2_key = SidePanelEntryKey(SidePanelEntryId::kBookmarks);
  std::unique_ptr<SidePanelEntry> entry_1 =
      CreateSidePanelEntry(entry_1_key, browser_);
  std::unique_ptr<SidePanelEntry> entry_2 =
      CreateSidePanelEntry(entry_2_key, browser_);
  TestSidePanelEntryObserver entry_1_observer(entry_1.get());
  TestSidePanelEntryObserver entry_2_observer(entry_2.get());

  auto* registry = SidePanelRegistry::From(browser_);
  registry->Register(std::move(entry_1));
  registry->Register(std::move(entry_2));

  // Act: Open the side panel, then immediately show a different entry.
  coordinator_->SidePanelUIBase::Show(entry_1_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/false);
  coordinator_->SidePanelUIBase::Show(entry_2_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/false);
  WaitUntilOpened(coordinator_);

  // Assert:
  EXPECT_EQ(entry_1_key.id(),
            entry_1_observer.id_for_last_entry_shown_.value());
  EXPECT_EQ(entry_1_key.id(),
            entry_1_observer.id_for_last_entry_hidden_.value());
  EXPECT_EQ(SidePanelEntryHideReason::kReplaced,
            entry_1_observer.reason_for_last_entry_hidden_with_reason_.value());
  EXPECT_EQ(entry_2_key.id(),
            entry_2_observer.id_for_last_entry_shown_.value());
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    TestAnimations_ShowThenShow_SameEntry_WaitForAnimationsToEnd_SecondShowIsNoOp) {
  // Arrange:
  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser_);
  TestSidePanelEntryObserver entry_observer(entry.get());

  auto* registry = SidePanelRegistry::From(browser_);
  registry->Register(std::move(entry));

  // Act: Open the side panel, wait for animations to end, then show the same
  // entry.
  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/false);
  WaitUntilOpened(coordinator_);
  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/false);
  WaitUntilOpened(coordinator_);

  // Assert:
  EXPECT_EQ(entry_key.id(), entry_observer.id_for_last_entry_shown_.value());
  EXPECT_EQ(1, entry_observer.num_on_entry_shown_received_);
  EXPECT_FALSE(entry_observer.id_for_last_entry_hidden_.has_value());
  EXPECT_FALSE(
      entry_observer.reason_for_last_entry_hidden_with_reason_.has_value());
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    TestAnimations_ShowThenShow_SameEntry_DoNotWaitForAnimationsToEnd_SecondShowIsNoOp) {
  // Arrange:
  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser_);
  TestSidePanelEntryObserver entry_observer(entry.get());

  auto* registry = SidePanelRegistry::From(browser_);
  registry->Register(std::move(entry));

  // Act: Open the side panel, then immediately show the same entry again.
  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/false);
  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/false);
  WaitUntilOpened(coordinator_);

  // Assert:
  EXPECT_EQ(entry_key.id(), entry_observer.id_for_last_entry_shown_.value());
  EXPECT_EQ(1, entry_observer.num_on_entry_shown_received_);
  EXPECT_FALSE(entry_observer.id_for_last_entry_hidden_.has_value());
  EXPECT_FALSE(
      entry_observer.reason_for_last_entry_hidden_with_reason_.has_value());
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    TestAnimations_CloseThenClose_WaitForAnimationsToEnd_SecondCloseIsNoOp) {
  // Arrange:
  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser_);
  TestSidePanelEntryObserver entry_observer(entry.get());

  auto* registry = SidePanelRegistry::From(browser_);
  registry->Register(std::move(entry));

  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);

  // Act: Close the side panel, wait for animations to end, then close again.
  coordinator_->Close(SidePanelEntryHideReason::kSidePanelClosed,
                      /*suppress_animations=*/false);
  WaitUntilClosed(coordinator_);
  coordinator_->Close(SidePanelEntryHideReason::kSidePanelClosed,
                      /*suppress_animations=*/false);
  WaitUntilClosed(coordinator_);

  // Assert:
  EXPECT_EQ(entry_key.id(), entry_observer.id_for_last_entry_hidden_.value());
  EXPECT_EQ(SidePanelEntryHideReason::kSidePanelClosed,
            entry_observer.reason_for_last_entry_hidden_with_reason_.value());
  EXPECT_EQ(1, entry_observer.num_on_entry_hidden_received_);
  EXPECT_EQ(1, entry_observer.num_on_entry_hidden_with_reason_received_);
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    TestAnimations_CloseThenClose_DoNotWaitForAnimations_SecondCloseIsNoOp) {
  // Arrange:
  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry =
      CreateSidePanelEntry(entry_key, browser_);
  TestSidePanelEntryObserver entry_observer(entry.get());

  auto* registry = SidePanelRegistry::From(browser_);
  registry->Register(std::move(entry));

  coordinator_->SidePanelUIBase::Show(entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);

  // Act: Close the side panel, then immediately close again.
  coordinator_->Close(SidePanelEntryHideReason::kSidePanelClosed,
                      /*suppress_animations=*/false);
  coordinator_->Close(SidePanelEntryHideReason::kSidePanelClosed,
                      /*suppress_animations=*/false);
  WaitUntilClosed(coordinator_);

  // Assert:
  EXPECT_EQ(entry_key.id(), entry_observer.id_for_last_entry_hidden_.value());
  EXPECT_EQ(SidePanelEntryHideReason::kSidePanelClosed,
            entry_observer.reason_for_last_entry_hidden_with_reason_.value());
  EXPECT_EQ(1, entry_observer.num_on_entry_hidden_received_);
  EXPECT_EQ(1, entry_observer.num_on_entry_hidden_with_reason_received_);
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    TestCrossRegistryEntryReplacement_WindowScopedEntryReplacesTabScopedEntry_TabScopedEntryBecomesInactive) {
  // Arrange: Open 2 tabs.
  tabs::TabInterface* tab_1 = tab_list_->GetActiveTab();
  tabs::TabInterface* tab_2 =
      tab_list_->OpenTab(GURL("about:blank"), /*index=*/1);

  // Arrange: Register a tab_1-scoped entry.
  auto tab_1_scoped_entry_key =
      SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  SidePanelRegistry::From(tab_1)->Register(
      CreateSidePanelEntry(tab_1_scoped_entry_key, browser_));

  // Arrange: Register a window-scoped entry.
  auto window_scoped_entry_key =
      SidePanelEntryKey(SidePanelEntryId::kBookmarks);
  SidePanelRegistry::From(browser_)->Register(
      CreateSidePanelEntry(window_scoped_entry_key, browser_));

  // Arrange: On tab_1, open the tab-scoped entry.
  tab_list_->ActivateTab(tab_1->GetHandle());
  coordinator_->SidePanelUIBase::Show(tab_1_scoped_entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  EXPECT_EQ(tab_1_scoped_entry_key.id(), coordinator_->GetCurrentEntryId());

  // Act: On tab_1, open the window-scoped entry. This will replace the
  // tab-scoped entry.
  coordinator_->SidePanelUIBase::Show(window_scoped_entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  EXPECT_EQ(window_scoped_entry_key.id(), coordinator_->GetCurrentEntryId());

  // Act: Switch to tab_2, the window-scoped entry is still shown.
  tab_list_->ActivateTab(tab_2->GetHandle());
  EXPECT_EQ(window_scoped_entry_key.id(), coordinator_->GetCurrentEntryId());

  // Act: Close the window-scoped entry.
  coordinator_->Close(SidePanelEntryHideReason::kSidePanelClosed,
                      /*suppress_animations=*/true);
  WaitUntilClosed(coordinator_);

  // Act: Switch to tab_1.
  tab_list_->ActivateTab(tab_1->GetHandle());

  // Assert: tab_1's tab-scoped entry is NOT re-shown, since it
  // became inactive when it was replaced by the window-scoped entry.
  EXPECT_FALSE(coordinator_->IsSidePanelShowing());
}

IN_PROC_BROWSER_TEST_F(
    SidePanelCoordinatorAndroidBrowserTest,
    TestCrossRegistryEntryReplacement_TabScopedEntryReplacesWindowScopedEntry_WindowScopedEntryRemainsActive) {
  // Arrange: Open 2 tabs.
  tabs::TabInterface* tab_1 = tab_list_->GetActiveTab();
  tabs::TabInterface* tab_2 =
      tab_list_->OpenTab(GURL("about:blank"), /*index=*/1);

  // Arrange: Register a tab_1-scoped entry.
  auto tab_1_scoped_entry_key =
      SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  SidePanelRegistry::From(tab_1)->Register(
      CreateSidePanelEntry(tab_1_scoped_entry_key, browser_));

  // Arrange: Register a window-scoped entry.
  auto window_scoped_entry_key =
      SidePanelEntryKey(SidePanelEntryId::kBookmarks);
  SidePanelRegistry::From(browser_)->Register(
      CreateSidePanelEntry(window_scoped_entry_key, browser_));

  // Arrange: Open tab_1 and the window-scoped entry.
  tab_list_->ActivateTab(tab_1->GetHandle());
  coordinator_->SidePanelUIBase::Show(window_scoped_entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  EXPECT_EQ(window_scoped_entry_key.id(), coordinator_->GetCurrentEntryId());

  // Act: In the same tab, open the tab-scoped entry. This will replace the
  // window-scoped entry.
  coordinator_->SidePanelUIBase::Show(tab_1_scoped_entry_key,
                                      SidePanelOpenTrigger::kToolbarButton,
                                      /*suppress_animations=*/true);
  WaitUntilOpened(coordinator_);
  EXPECT_EQ(tab_1_scoped_entry_key.id(), coordinator_->GetCurrentEntryId());

  // Act: Open tab_2. The window-scoped entry should re-appear. This because the
  // window-scoped entry remains active even if it's replaced in tab_1.
  tab_list_->ActivateTab(tab_2->GetHandle());
  WaitUntilOpened(coordinator_);
  EXPECT_EQ(window_scoped_entry_key.id(), coordinator_->GetCurrentEntryId());

  // Act: Go back to tab_1.
  tab_list_->ActivateTab(tab_1->GetHandle());

  // Assert: Tab_1's tab-scoped entry should re-appear.
  EXPECT_EQ(tab_1_scoped_entry_key.id(), coordinator_->GetCurrentEntryId());
}

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       RapidTabSwitch_AutoOpenSidePanel) {
  coordinator_->ConfigDeferredViewReplacementForTesting(true);

  // Simulate GLiC's "automatically open side panel for a new tab" behavior.
  AutoOpenSidePanelTabModelObserver observer(browser_, coordinator_);
  auto* tab_model = static_cast<TabModel*>(tab_list_);
  tab_model->AddObserver(&observer);

  const auto& side_panel_entry_observers =
      observer.side_panel_entry_observers();

  // Tab 2: Opens the side panel initially.
  tab_list_->OpenTab(GURL("about:blank"), tab_list_->GetTabCount());
  ASSERT_EQ(1u, side_panel_entry_observers.size());
  EXPECT_EQ(1, side_panel_entry_observers[0]->num_on_entry_shown_received_);

  // Tab 3: Replaces Tab 2's panel (1st replacement).
  // In a real device, this uses delayed detachment. In the headless test
  // environment, it may complete synchronously.
  tab_list_->OpenTab(GURL("about:blank"), tab_list_->GetTabCount());
  ASSERT_EQ(2u, side_panel_entry_observers.size());
  EXPECT_EQ(1, side_panel_entry_observers[1]->num_on_entry_shown_received_);

  // Tab 4: Replaces Tab 3's panel (2nd replacement).
  tab_list_->OpenTab(GURL("about:blank"), tab_list_->GetTabCount());
  ASSERT_EQ(3u, side_panel_entry_observers.size());
  EXPECT_EQ(1, side_panel_entry_observers[2]->num_on_entry_shown_received_);

  // Wait for the final tab's panel to open.
  WaitUntilOpened(coordinator_);
  EXPECT_EQ(SidePanelEntryId::kGlic, coordinator_->GetCurrentEntryId());

  // Wait for any background delayed detachments (if any) to finish
  // successfully.
  ASSERT_TRUE(base::test::RunUntil(
      [this]() { return !coordinator_->HasPendingReplacedEntryForTesting(); }));

  // At this point, the older tabs should be hidden and the active tab shown.
  EXPECT_EQ(1, side_panel_entry_observers[0]->num_on_entry_hidden_received_);
  EXPECT_EQ(1, side_panel_entry_observers[1]->num_on_entry_hidden_received_);
  EXPECT_EQ(0, side_panel_entry_observers[2]->num_on_entry_hidden_received_);

  tab_model->RemoveObserver(&observer);
  coordinator_->ConfigDeferredViewReplacementForTesting(false);
}
