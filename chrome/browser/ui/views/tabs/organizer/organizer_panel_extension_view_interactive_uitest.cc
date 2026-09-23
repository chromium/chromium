// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/animation/browser_animation_controller.h"
#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extension_side_panel_utils.h"
#include "chrome/browser/ui/views/animations/organizer_panel_animations.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"

namespace {
constexpr int kBrowserWindowWidth = 1400;
constexpr int kBrowserWindowHeight = 800;

DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kShowAnimationComplete);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kHideAnimationComplete);

base::CallbackListSubscription SubscribeToAnimations(
    BrowserWindowInterface* browser) {
  return BrowserAnimationController::From(browser)->Subscribe(
      OrganizerPanelAnimations::kOrganizerPanel,
      base::BindLambdaForTesting(
          [browser](const BrowserAnimationController* controller,
                    BrowserAnimationUpdate update) {
            if (update == BrowserAnimationUpdate::kEnded) {
              const auto motion = controller->GetCurrentMotion(
                  OrganizerPanelAnimations::kOrganizerPanel);
              auto* const browser_view =
                  BrowserView::GetBrowserViewForBrowser(browser);
              if (motion == OrganizerPanelAnimations::kShow) {
                views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
                    kShowAnimationComplete, browser_view);
              } else if (motion == OrganizerPanelAnimations::kHide) {
                views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
                    kHideAnimationComplete, browser_view);
              }
            }
          }));
}

}  // namespace

class OrganizerPanelExtensionInteractiveUiTest
    : public InteractiveBrowserTestMixin<extensions::ExtensionBrowserTest> {
 public:
  OrganizerPanelExtensionInteractiveUiTest() {
    scoped_feature_list_.InitWithFeatures(
        {organizer_panel::kOrganizerPanel,
         organizer_panel::kShowExtensionsSidePanelUiInOrganizerPanel},
        {});
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTestMixin<
        extensions::ExtensionBrowserTest>::SetUpOnMainThread();

    browser()->GetProfile()->GetPrefs()->SetBoolean(
        prefs::kTabSearchPinnedToTabstrip, true);

    browser()->GetWindow()->SetBounds(
        gfx::Rect(0, 0, kBrowserWindowWidth, kBrowserWindowHeight));

    animation_subscription_ = SubscribeToAnimations(browser());
  }

  void TearDownOnMainThread() override {
    animation_subscription_ = base::CallbackListSubscription();
    InteractiveBrowserTestMixin::TearDownOnMainThread();
  }

  const extensions::Extension* LoadExtensionWithSidePanel(
      const std::string& name = "Test Extension") {
    auto dir = std::make_unique<extensions::TestExtensionDir>();
    constexpr std::string_view kManifest =
        R"({
             "name": "%s",
             "version": "0.1",
             "manifest_version": 3,
             "side_panel": {
               "default_path": "side_panel.html"
             }
           })";
    dir->WriteManifest(base::StringPrintf(kManifest, name.c_str()));
    dir->WriteFile(FILE_PATH_LITERAL("side_panel.html"),
                   "<html><body>Side Panel Content</body></html>");
    const extensions::Extension* extension = LoadExtension(dir->UnpackedPath());
    extension_dirs_.push_back(std::move(dir));
    return extension;
  }

  auto WaitForPanelOpen() {
    return InParallel(RunSubsequence(WaitForEvent(kBrowserViewElementId,
                                                  kShowAnimationComplete)),
                      RunSubsequence(WaitForShow(kOrganizerPanelElementId)))
        .SetDescription("WaitForPanelOpen()");
  }

  auto WaitForPanelClose() {
    return InParallel(RunSubsequence(WaitForEvent(kBrowserViewElementId,
                                                  kHideAnimationComplete)),
                      RunSubsequence(WaitForHide(kOrganizerPanelElementId)))
        .SetDescription("WaitForPanelClose()");
  }

  auto CheckControllerState(
      bool visible,
      const extensions::ExtensionId& id = extensions::ExtensionId()) {
    auto steps = Steps(CheckResult(
        [this]() {
          return organizer_panel_controller()->IsOrganizerPanelVisible();
        },
        visible));
    if (!id.empty()) {
      steps += CheckResult(
          [this]() {
            return organizer_panel_controller()->active_extension_id();
          },
          id);
    }
    AddDescriptionPrefix(steps, "CheckControllerState()");
    return steps;
  }

  OrganizerPanelController* organizer_panel_controller() {
    return OrganizerPanelController::From(browser());
  }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::vector<std::unique_ptr<extensions::TestExtensionDir>> extension_dirs_;
  base::CallbackListSubscription animation_subscription_;
};

IN_PROC_BROWSER_TEST_F(OrganizerPanelExtensionInteractiveUiTest,
                       OpensExtensionSidePanelInOrganizerPanel) {
  const extensions::Extension* extension = LoadExtensionWithSidePanel();
  ASSERT_TRUE(extension);

  RunTestSequence(CheckControllerState(false), Do([this, extension]() {
                    extensions::side_panel_util::OpenGlobalExtensionSidePanel(
                        *browser(), /*web_contents=*/nullptr, extension->id());
                  }),
                  WaitForPanelOpen(),
                  CheckControllerState(true, extension->id()),
                  WaitForShow(OrganizerPanelView::kWebViewElementId));
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelExtensionInteractiveUiTest,
                       TogglesExtensionSidePanelInOrganizerPanel) {
  const extensions::Extension* extension = LoadExtensionWithSidePanel();
  ASSERT_TRUE(extension);

  RunTestSequence(
      // Toggle to open.
      Do([this, extension]() {
        extensions::side_panel_util::ToggleExtensionSidePanel(browser(),
                                                              extension->id());
      }),
      WaitForPanelOpen(), CheckControllerState(true, extension->id()),
      // Toggle to close.
      Do([this, extension]() {
        extensions::side_panel_util::ToggleExtensionSidePanel(browser(),
                                                              extension->id());
      }),
      WaitForPanelClose(), CheckControllerState(false));
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelExtensionInteractiveUiTest,
                       ClosesExtensionSidePanelInOrganizerPanel) {
  const extensions::Extension* extension = LoadExtensionWithSidePanel();
  ASSERT_TRUE(extension);

  RunTestSequence(Do([this, extension]() {
                    extensions::side_panel_util::OpenGlobalExtensionSidePanel(
                        *browser(), /*web_contents=*/nullptr, extension->id());
                  }),
                  WaitForPanelOpen(),
                  CheckControllerState(true, extension->id()),
                  Do([this, extension]() {
                    extensions::side_panel_util::CloseGlobalExtensionSidePanel(
                        browser(), extension->id());
                  }),
                  WaitForPanelClose(), CheckControllerState(false));
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelExtensionInteractiveUiTest,
                       SwitchesBetweenExtensionsInOrganizerPanel) {
  const extensions::Extension* ext1 = LoadExtensionWithSidePanel("Ext 1");
  const extensions::Extension* ext2 = LoadExtensionWithSidePanel("Ext 2");
  ASSERT_TRUE(ext1);
  ASSERT_TRUE(ext2);

  RunTestSequence(
      // Open extension 1.
      Do([this, ext1]() {
        extensions::side_panel_util::OpenGlobalExtensionSidePanel(
            *browser(), /*web_contents=*/nullptr, ext1->id());
      }),
      WaitForPanelOpen(), CheckControllerState(true, ext1->id()),
      // Open extension 2 (should switch content seamlessly).
      Do([this, ext2]() {
        extensions::side_panel_util::OpenGlobalExtensionSidePanel(
            *browser(), /*web_contents=*/nullptr, ext2->id());
      }),
      CheckControllerState(true, ext2->id()),
      WaitForShow(OrganizerPanelView::kWebViewElementId));
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelExtensionInteractiveUiTest,
                       OpensDefaultExtensionWhenOpenedWithoutExtensionId) {
  const extensions::Extension* extension = LoadExtensionWithSidePanel();
  ASSERT_TRUE(extension);

  RunTestSequence(
      // Open organizer panel without specifying an extension ID.
      Do([this]() { organizer_panel_controller()->SetOrganizerVisible(true); }),
      WaitForPanelOpen(), WaitForShow(OrganizerPanelView::kWebViewElementId));
}
