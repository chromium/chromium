// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/internal/android/side_panel_coordinator_android.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/side_panel/android/android_side_panel_enabled_fn.h"
#include "chrome/browser/ui/side_panel/android/side_panel_native_view_android.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/side_panel/side_panel_ui_provider.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class TestSidePanelEntryObserver final : public SidePanelEntryObserver {
 public:
  TestSidePanelEntryObserver() = default;
  ~TestSidePanelEntryObserver() override = default;

  void OnEntryShown(SidePanelEntry* entry) override {
    id_for_last_entry_shown_ = entry->key().id();
  }

  std::optional<SidePanelEntry::Id> id_for_last_entry_shown_;
};

std::unique_ptr<SidePanelEntry> CreateSidePanelEntry(SidePanelEntryKey key) {
  SidePanelEntry::CreateContentCallback create_content_callback =
      base::BindRepeating([](SidePanelEntryScope& scope) {
        base::android::ScopedJavaGlobalRef<jobject> java_view;
        return std::make_unique<SidePanelNativeViewAndroid>(
            std::move(java_view));
      });

  auto default_content_width_callback = base::RepeatingCallback<int()>();

  return std::make_unique<SidePanelEntry>(key, create_content_callback,
                                          default_content_width_callback);
}

BrowserWindowInterface* GetBrowserWindow() {
  std::vector<BrowserWindowInterface*> windows =
      GetAllBrowserWindowInterfaces();
  EXPECT_EQ(1u, windows.size())
      << "We don't expect more than one window in this test.";
  return windows[0];
}
}  // namespace

class SidePanelCoordinatorAndroidBrowserTest : public AndroidBrowserTest {
 public:
  SidePanelCoordinatorAndroidBrowserTest() {
    // Note:
    //
    // Java code reads the cached `kEnableAndroidSidePanel` flag because we
    // use this flag to decide whether to inflate the main layout that contains
    // the side panel container, which happens _before_ the native library is
    // loaded.
    //
    // As of Mar 25, 2026, there was no way to override a cached flag in native
    // browser tests, so http://crrev.com/c/7689838 made the default value of
    // the cached flag `true` in tests.
    //
    // However, we still need to explicitly enable the flag here:
    //
    // On a newly installed ChromeBrowserTests APK, the `SharedPreferences`
    // backing the cached flag is empty so http://crrev.com/c/7689838 makes the
    // test pass the 1st run.
    //
    // After the 1st run, the `SharedPreferences` will contain the key for the
    // cached flag, but the default value of the cached flag won't be persisted.
    // If we don't explicitly enable the flag here, the cached flag value will
    // be `false` on subsequent runs and the tests will fail.
    feature_list_.InitAndEnableFeature(
        chrome::android::kEnableAndroidSidePanel);
  }

  ~SidePanelCoordinatorAndroidBrowserTest() override = default;

  void SetUp() override {
    // Despite the flag setup in the constructor, not all bots can see the
    // flag's "default value in tests". For example, bots with
    // "is_chrome_branded=true" don't read the "default value in tests". For
    // more details, please see `CachedFlag.java`.
    //
    // Here we'll do a final check of the flag and skip all tests if the flag
    // isn't enabled in tests.
    if (!AndroidSidePanelEnabledFn::IsEnabled()) {
      GTEST_SKIP() << "Android Side Panel is disabled";
    }
    AndroidBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
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
  std::unique_ptr<SidePanelEntry> entry = CreateSidePanelEntry(entry_key);
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
                       IsSidePanelEntryShowing_AfterShow_ReturnsTrue) {
  // Arrange:
  BrowserWindowInterface* browser = GetBrowserWindow();

  auto entry_key = SidePanelEntryKey(SidePanelEntryId::kAboutThisSite);
  std::unique_ptr<SidePanelEntry> entry = CreateSidePanelEntry(entry_key);
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
