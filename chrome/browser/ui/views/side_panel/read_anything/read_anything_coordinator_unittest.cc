// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/accessibility/embedded_a11y_extension_loader.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_web_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_ui.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/accessibility_features.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/embedded_a11y_manager_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

using testing::_;

class MockReadAnythingCoordinatorObserver
    : public ReadAnythingCoordinator::Observer {
 public:
  MOCK_METHOD(void, Activate, (bool active), (override));
  MOCK_METHOD(void, OnActivePageDistillable, (bool distillable), (override));
};

class ReadAnythingCoordinatorTest : public TestWithBrowserView {
 public:
  ReadAnythingCoordinatorTest()
      : TestWithBrowserView(
            content::BrowserTaskEnvironment::TimeSource::MOCK_TIME) {}

  ReadAnythingCoordinatorTest(const ReadAnythingCoordinatorTest&) = delete;
  ReadAnythingCoordinatorTest& operator=(const ReadAnythingCoordinatorTest&) =
      delete;

  void SetUp() override {
    base::test::ScopedFeatureList features;
    scoped_feature_list_.InitWithFeatures(
        {
            features::kReadAnythingDocsIntegration,
        },
        {});
    TestWithBrowserView::SetUp();

    InitExtensionSystem(profile());

    side_panel_coordinator_ = browser()->GetFeatures().side_panel_coordinator();
    read_anything_coordinator_ =
        browser()->GetFeatures().read_anything_coordinator();

    // Ensure a kReadAnything entry is added to the contextual registry for the
    // first tab.
    AddTabToBrowser(GURL("http://foo1.com"));
    auto* tab_one_registry = SidePanelRegistry::GetDeprecated(
        browser_view()->GetActiveWebContents());
    contextual_registries_.push_back(tab_one_registry);

    // Ensure a kReadAnything entry is added to the contextual registry for the
    // second tab.
    AddTabToBrowser(GURL("http://foo2.com"));
    auto* tab_two_registry = SidePanelRegistry::GetDeprecated(
        browser_view()->GetActiveWebContents());
    contextual_registries_.push_back(tab_two_registry);

    // Verify the first tab has one entry, kReadAnything.
    browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
    SidePanelRegistry* contextual_registry = SidePanelRegistry::GetDeprecated(
        browser_view()->GetActiveWebContents());
    ASSERT_EQ(contextual_registry->entries().size(), 1u);
    EXPECT_EQ(contextual_registry->entries()[0]->key().id(),
              SidePanelEntry::Id::kReadAnything);

    // Verify the second tab has one entry, kReadAnything.
    browser_view()->browser()->tab_strip_model()->ActivateTabAt(1);
    contextual_registry = SidePanelRegistry::GetDeprecated(
        browser_view()->GetActiveWebContents());
    ASSERT_EQ(contextual_registry->entries().size(), 1u);
    EXPECT_EQ(contextual_registry->entries()[0]->key().id(),
              SidePanelEntry::Id::kReadAnything);
  }

  void InitExtensionSystem(Profile* profile) {
    extensions::TestExtensionSystem* extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile));
    extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
  }

  // Wrapper methods around the ReadAnythingCoordinator. These do nothing more
  // than keep the below tests less verbose (simple pass-throughs).

  void AddObserver(ReadAnythingCoordinator::Observer* observer) {
    read_anything_coordinator_->AddObserver(observer);
  }
  void RemoveObserver(ReadAnythingCoordinator::Observer* observer) {
    read_anything_coordinator_->RemoveObserver(observer);
  }
  std::unique_ptr<views::View> CreateContainerView() {
    return read_anything_coordinator_->CreateContainerView();
  }

  void OnBrowserSetLastActive(Browser* browser) {
    read_anything_coordinator_->OnBrowserSetLastActive(browser);
  }

  void ActivePageDistillable() {
    read_anything_coordinator_->ActivePageDistillable();
  }

  void ActivePageNotDistillable() {
    read_anything_coordinator_->ActivePageNotDistillable();
  }

  void AddTabToBrowser(const GURL& tab_url) {
    AddTab(browser_view()->browser(), tab_url);
    // Remove the companion entry if it present.
    auto* registry = SidePanelRegistry::GetDeprecated(
        browser_view()->GetActiveWebContents());
    registry->Deregister(
        SidePanelEntry::Key(SidePanelEntry::Id::kSearchCompanion));
  }

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  bool IsGDocsHelperExtensionLoaded() {
#if BUILDFLAG(IS_CHROMEOS)
    return EmbeddedA11yExtensionLoader::GetInstance()->IsExtensionInstalled(
        extension_misc::kReadingModeGDocsHelperExtensionId);
#else
    extensions::ComponentLoader* component_loader =
        extensions::ExtensionSystem::Get(profile())
            ->extension_service()
            ->component_loader();
    return component_loader->Exists(
        extension_misc::kReadingModeGDocsHelperExtensionId);
#endif  // BUILDFLAG(IS_CHROMEOS)
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

 protected:
  content::ScopedWebUIConfigRegistration webui_config_registration_{
      std::make_unique<ReadAnythingUIUntrustedConfig>()};

  raw_ptr<SidePanelCoordinator, DanglingUntriaged> side_panel_coordinator_ =
      nullptr;
  std::vector<raw_ptr<SidePanelRegistry, DanglingUntriaged>>
      contextual_registries_;
  raw_ptr<ReadAnythingCoordinator, DanglingUntriaged>
      read_anything_coordinator_ = nullptr;

  MockReadAnythingCoordinatorObserver coordinator_observer_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/40853217): Fix the memory leak on destruction observed on
// these tests on asan mac.
#if !BUILDFLAG(IS_MAC) || !defined(ADDRESS_SANITIZER)

TEST_F(ReadAnythingCoordinatorTest, ContainerViewsAreUnique) {
  auto view1 = CreateContainerView();
  auto view2 = CreateContainerView();
  EXPECT_NE(view1, view2);
}

TEST_F(ReadAnythingCoordinatorTest,
       ActivateCalled_ShowAndHideReadAnythingEntry) {
  AddObserver(&coordinator_observer_);
  ASSERT_EQ(contextual_registries_.size(), 2u);
  SidePanelEntry* entry = contextual_registries_[0]->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));

  EXPECT_CALL(coordinator_observer_, Activate(true)).Times(1);
  entry->OnEntryShown();

  EXPECT_CALL(coordinator_observer_, Activate(false)).Times(1);
  entry->OnEntryHidden();
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(ReadAnythingCoordinatorTest,
       SidePanelShowAndHide_NonLacros_CallEmbeddedA11yExtensionLoader) {
  SidePanelEntry* entry = contextual_registries_[0]->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));
  EXPECT_FALSE(IsGDocsHelperExtensionLoaded());

  // If the local side panel entry is shown, install the helper extension.
  entry->OnEntryShown();
  EXPECT_TRUE(IsGDocsHelperExtensionLoaded());

  // If the local side panel entry is hidden, remove the helper extension after
  // a timeout.
  entry->OnEntryHidden();
  // The helper extension is not removed immediately.
  EXPECT_TRUE(IsGDocsHelperExtensionLoaded());
  // The helper extension is removed after a timeout.
  task_environment()->FastForwardBy(base::Seconds(30));
  EXPECT_FALSE(IsGDocsHelperExtensionLoaded());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(
    ReadAnythingCoordinatorTest,
    SidePanelShowAndHide_Lacros_EmbeddedA11yManagerLacrosUpdateReadingModeState) {
  SidePanelEntry* entry = contextual_registries_[0]->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));
  EXPECT_FALSE(
      EmbeddedA11yManagerLacros::GetInstance()->IsReadingModeEnabled());

  // If the local side panel entry is shown, set reading mode enabled to true.
  entry->OnEntryShown();
  EXPECT_TRUE(EmbeddedA11yManagerLacros::GetInstance()->IsReadingModeEnabled());

  // If the local side panel entry is hidden, set reading mode enabled to false
  // after a timeout.
  entry->OnEntryHidden();
  // The reading mode setting is not updated immediately.
  EXPECT_TRUE(EmbeddedA11yManagerLacros::GetInstance()->IsReadingModeEnabled());
  // The reading mode setting is updated after a timeout.
  task_environment()->FastForwardBy(base::Seconds(30));
  EXPECT_FALSE(
      EmbeddedA11yManagerLacros::GetInstance()->IsReadingModeEnabled());
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

TEST_F(ReadAnythingCoordinatorTest,
       OnBrowserSetLastActive_SidePanelIsNotVisible) {
  Browser* browser = browser_view()->browser();
  OnBrowserSetLastActive(browser);

  EXPECT_FALSE(side_panel_coordinator_->IsSidePanelShowing());
}

TEST_F(ReadAnythingCoordinatorTest, OnActivePageDistillableCalled) {
  AddObserver(&coordinator_observer_);

  EXPECT_CALL(coordinator_observer_, OnActivePageDistillable(true)).Times(1);
  // Called once when calling ActivePageDistillable and once on destruction.
  EXPECT_CALL(coordinator_observer_, OnActivePageDistillable(false)).Times(2);

  ActivePageDistillable();
  ActivePageNotDistillable();
}

TEST_F(ReadAnythingCoordinatorTest, WithWebUIFlagEnabled_ShowsWebUIToolbar) {
  ASSERT_STREQ("ReadAnythingSidePanelWebView",
               CreateContainerView()->GetClassName());
}

class ReadAnythingCoordinatorScreen2xDataCollectionModeTest
    : public TestWithBrowserView {
 public:
  void SetUp() override {
    base::test::ScopedFeatureList features;
    scoped_feature_list_.InitWithFeatures(
        {features::kDataCollectionModeForScreen2x}, {});
    TestWithBrowserView::SetUp();

    side_panel_coordinator_ = browser()->GetFeatures().side_panel_coordinator();
    read_anything_coordinator_ =
        browser()->GetFeatures().read_anything_coordinator();

    AddTab(browser_view()->browser(), GURL("http://foo1.com"));
    browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
  }

  void OnBrowserSetLastActive(Browser* browser) {
    read_anything_coordinator_->OnBrowserSetLastActive(browser);
  }

 protected:
  content::ScopedWebUIConfigRegistration webui_config_registration_{
      std::make_unique<ReadAnythingUIUntrustedConfig>()};

  raw_ptr<SidePanelCoordinator, DanglingUntriaged> side_panel_coordinator_ =
      nullptr;
  raw_ptr<ReadAnythingCoordinator, DanglingUntriaged>
      read_anything_coordinator_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ReadAnythingCoordinatorScreen2xDataCollectionModeTest,
       OnBrowserSetLastActive_SidePanelIsVisible) {
  Browser* browser = browser_view()->browser();
  OnBrowserSetLastActive(browser);

  EXPECT_TRUE(side_panel_coordinator_->IsSidePanelShowing());
  EXPECT_EQ(browser->GetFeatures().side_panel_ui()->GetCurrentEntryId(),
            SidePanelEntryId::kReadAnything);
}

#endif  // !defined(ADDRESS_SANITIZER)
