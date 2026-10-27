// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_entry_point_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"
#include "chrome/browser/ui/read_anything/read_anything_prefs.h"
#include "chrome/browser/ui/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"

using read_anything::mojom::ReadAnythingOpenTrigger;

namespace {
constexpr char kDocumentWithNamedElement[] = "/select.html";
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kReadAnythingWebContentsId);
}  // namespace

class MockReadAnythingLifecycleObserver : public ReadAnythingLifecycleObserver {
 public:
  MOCK_METHOD(void,
              Activate,
              (bool active,
               ReadAnythingOpenTrigger,
               std::optional<base::TimeDelta>),
              (override));
  MOCK_METHOD(void, OnDestroyed, (), (override));
};

class ReadAnythingSidePanelControllerTest : public InProcessBrowserTest {
 public:
  void TearDownOnMainThread() override {
    RemoveObserver(&read_anything_observer_);
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void AddObserver(ReadAnythingLifecycleObserver* observer) {
    ReadAnythingController::From(browser()->GetActiveTabInterface())
        ->AddObserver(observer);
  }
  void RemoveObserver(ReadAnythingLifecycleObserver* observer) {
    ReadAnythingController::From(browser()->GetActiveTabInterface())
        ->RemoveObserver(observer);
  }

 protected:
  MockReadAnythingLifecycleObserver read_anything_observer_;
};

IN_PROC_BROWSER_TEST_F(ReadAnythingSidePanelControllerTest,
                       RegisterReadAnythingEntry) {
  // The tab should have a read anything entry in its side panel.
  EXPECT_EQ(SidePanelRegistry::From(browser()->GetActiveTabInterface())
                ->GetEntryForKey(
                    SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything))
                ->key()
                .id(),
            SidePanelEntry::Id::kReadAnything);
}

IN_PROC_BROWSER_TEST_F(ReadAnythingSidePanelControllerTest,
                       OnEntryShown_ActivateObservers) {
  AddObserver(&read_anything_observer_);
  SidePanelEntry* entry =
      SidePanelRegistry::From(browser()->GetActiveTabInterface())
          ->GetEntryForKey(
              SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));
  entry->set_last_open_trigger(SidePanelOpenTrigger::kReadAnythingOmniboxChip);

  EXPECT_CALL(read_anything_observer_,
              Activate(true, ReadAnythingOpenTrigger::kOmniboxChip, testing::_))
      .Times(1);
  entry->OnEntryShown();
}

IN_PROC_BROWSER_TEST_F(ReadAnythingSidePanelControllerTest,
                       OnEntryHidden_ActivateObservers) {
  AddObserver(&read_anything_observer_);
  SidePanelEntry* entry =
      SidePanelRegistry::From(browser()->GetActiveTabInterface())
          ->GetEntryForKey(
              SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));

  EXPECT_CALL(read_anything_observer_,
              Activate(false, ReadAnythingOpenTrigger::kUnknown, testing::_))
      .Times(1);
  entry->OnEntryHidden();
}

class ReadAnythingSidePanelControllerInteractiveTest
    : public InteractiveBrowserTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }
  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }
  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(
    ReadAnythingSidePanelControllerInteractiveTest,
    OpenImmersiveChangeToSidePanelAndCloseWithKeyboardShortcut) {
  ui::Accelerator reading_mode_accelerator;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_SHOW_READING_MODE_KEYBOARD, &reading_mode_accelerator));

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kReadingModeWebContentsId);
  RunTestSequence(
      InstrumentTab(kActiveTab),
      // Use a page with content. On an empty page, Reading Mode switches from
      // the immersive overlay to the side panel on its own.
      NavigateWebContents(
          kActiveTab, embedded_test_server()->GetURL("/long_text_page.html")),

      // Use the keyboard shortcut command to open the immersive overlay
      SendAccelerator(kBrowserViewElementId, reading_mode_accelerator),

      // Verify that presentation state is Immersive
      CheckResult(
          [this]() {
            return ReadAnythingController::From(
                       browser()->GetTabStripModel()->GetActiveTab())
                ->GetPresentationState();
          },
          ReadAnythingController::PresentationState::kInImmersiveOverlay),

      // Change presentation to Side Panel the same way Reading Mode's
      // presentation menu does.
      InAnyContext(WaitForShow(kReadAnythingSettingsButtonElementId)),
      InSameContext(InstrumentWebContentsContaining(
          kReadingModeWebContentsId, kReadAnythingSettingsButtonElementId)),
      InAnyContext(ExecuteJs(kReadingModeWebContentsId,
                             "() => chrome.readingMode.togglePresentation()")
                       .SetMustRemainVisible(false)),
      WaitForShow(kSidePanelElementId),

      // Verify that presentation state is Side Panel
      CheckResult(
          [this]() {
            return ReadAnythingController::From(
                       browser()->GetTabStripModel()->GetActiveTab())
                ->GetPresentationState();
          },
          ReadAnythingController::PresentationState::kInSidePanel),

      // Use the keyboard shortcut command again to close the side panel
      SendAccelerator(kBrowserViewElementId, reading_mode_accelerator),
      WaitForHide(kSidePanelElementId),

      // Verify that presentation state is Inactive
      CheckResult(
          [this]() {
            return ReadAnythingController::From(
                       browser()->GetTabStripModel()->GetActiveTab())
                ->GetPresentationState();
          },
          ReadAnythingController::PresentationState::kInactive));
}

class ReadAnythingSidePanelInteractiveTest
    : public ReadAnythingSidePanelControllerInteractiveTest {
 public:
  ReadAnythingSidePanelInteractiveTest() {
    feature_list_.InitAndEnableFeature(features::kReadAnythingImprovedUi);
  }
  ~ReadAnythingSidePanelInteractiveTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Regression test for https://crbug.com/557055820.
// Verifies that the "toggle images" menu button toggles the value in prefs.
IN_PROC_BROWSER_TEST_F(ReadAnythingSidePanelInteractiveTest, ToggleImages) {
  ui::Accelerator reading_mode_accelerator;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_SHOW_READING_MODE_KEYBOARD, &reading_mode_accelerator));

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);

  auto* const prefs = browser()->GetProfile()->GetPrefs();
  const bool old_setting =
      prefs->GetBoolean(prefs::kAccessibilityReadAnythingImagesEnabled);

  const DeepQuery kSettingsButtonQuery{"read-anything-app",
                                       "read-anything-toolbar", "#more"};
  const DeepQuery kMediaMenuQuery{"read-anything-app", "read-anything-toolbar",
                                  "settings-menu", "#media"};
  const DeepQuery kToggleImagesQuery{"read-anything-app",
                                     "read-anything-toolbar", "media-menu",
                                     "#images-toggle-button"};

  RunTestSequence(
      InstrumentTab(kActiveTab),
      NavigateWebContents(kActiveTab, embedded_test_server()->GetURL(
                                          kDocumentWithNamedElement)),

      // Show Read Anything side panel.
      Do([this]() {
        read_anything::ReadAnythingEntryPointController::ShowUI(
            browser(), ReadAnythingOpenTrigger::kOmniboxChip,
            read_anything::ReadAnythingEntryPointController::Presentation::
                kSidePanel);
      }),
      WaitForShow(kSidePanelElementId),
      // This is an element in the page.
      InAnyContext(WaitForShow(kReadAnythingSettingsButtonElementId)),
      InSameContext(InstrumentWebContentsContaining(
          kReadAnythingWebContentsId, kReadAnythingSettingsButtonElementId)),
      InAnyContext(
          WaitForElementVisible(kReadAnythingWebContentsId,
                                kSettingsButtonQuery),
          ClickElement(kReadAnythingWebContentsId, kSettingsButtonQuery),
          WaitForElementVisible(kReadAnythingWebContentsId, kMediaMenuQuery),
          ClickElement(kReadAnythingWebContentsId, kMediaMenuQuery),
          WaitForElementVisible(kReadAnythingWebContentsId, kToggleImagesQuery),
          ClickElement(kReadAnythingWebContentsId, kToggleImagesQuery),
          PollUntil(
              [prefs, old_setting]() {
                return prefs->GetBoolean(
                           prefs::kAccessibilityReadAnythingImagesEnabled) !=
                       old_setting;
              },
              "Wait for setting to change.")));
}
