// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/internal/android/side_panel_coordinator_android.h"

#include <memory>

#include "chrome/browser/ui/side_panel/android/side_panel_native_view_android.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/side_panel/side_panel_ui_provider.h"
#include "chrome/browser/ui/side_panel/test/android/side_panel_android_browser_test_base.h"
#include "content/public/test/browser_test.h"
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
