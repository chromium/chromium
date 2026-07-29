// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_tab_close_skip_beforeunload_user_data.h"
#include "chrome/browser/actor/ui/actor_task_unload_handler.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_web_contents_delegate/browser_web_contents_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/unload_controller.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

views::Widget* GetActorDialogWidget(UnloadController* controller) {
  for (const auto& h : controller->tab_unload_handlers_for_testing()) {
    if (auto* handler = static_cast<actor::ActorTaskUnloadHandler*>(h.get())) {
      if (auto* widget = handler->GetActiveDialogWidgetForTesting()) {
        return widget;
      }
    }
  }
  return nullptr;
}

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTabId);

// TODO(crbug.com/537847367): Migrate this test suite to GlicBrowserTest.
class UnloadControllerGlicInteractiveUiTest
    : public glic::test::InteractiveGlicTest {
 public:
  UnloadControllerGlicInteractiveUiTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlicConfirmTabClose, features::kGlic, features::kGlicActor,
         features::kGlicActorUi},
        {});
  }

 protected:
  void SetGlicAccessing(bool accessing) {
    if (accessing) {
      if (auto* unload_controller = UnloadController::From(browser())) {
        if (!unload_controller->HasTabUnloadHandlers()) {
          unload_controller->AddTabUnloadHandler(
              std::make_unique<::actor::ActorTaskUnloadHandler>());
        }
      }
    }
    ::actor::ActorTaskTabCloseConfirmDialog::SetShowAlwaysReturnsTrueForTesting(
        accessing);
  }

 private:
  raw_ptr<Profile, DisableDanglingPtrDetection> profile_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class UnloadControllerGlicDisabledInteractiveUiTest
    : public glic::test::InteractiveGlicTest {
 public:
  UnloadControllerGlicDisabledInteractiveUiTest() {
    scoped_feature_list_.InitWithFeatures({features::kGlic},
                                          {features::kGlicConfirmTabClose});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(UnloadControllerGlicInteractiveUiTest,
                       DialogAppearsWhenActive) {
  RunTestSequence(
      InstrumentTab(kActiveTabId),
      // DeprecatedOpenGlicWindow(glic::test::InteractiveGlicTest::kAttached),
      SelectTab(kTabStripElementId, 0), Do([&]() { SetGlicAccessing(true); }),
      Do([&]() {
        content::WebContents* web_contents =
            browser()->tab_strip_model()->GetWebContentsAt(0);
        BrowserWebContentsDelegate::From(browser())->CloseContents(
            web_contents);
      }),
      InAnyContext(WaitForShow(actor::ActorTaskTabCloseConfirmDialog::kViewId)),
      Check([&]() {
        auto* dialog = GetActorDialogWidget(UnloadController::From(browser()));
        if (!dialog) {
          return false;
        }
        SetGlicAccessing(false);
        dialog->widget_delegate()->AsDialogDelegate()->AcceptDialog();
        return true;
      }),
      // Cleanup so teardown closes the window normally
      Do([&]() { SetGlicAccessing(false); }));
}

IN_PROC_BROWSER_TEST_F(UnloadControllerGlicInteractiveUiTest, EndToEndLogTest) {
  RunTestSequence(
      InstrumentTab(kActiveTabId),
      // DeprecatedOpenGlicWindow(glic::test::InteractiveGlicTest::kAttached),
      SelectTab(kTabStripElementId, 0), Do([&]() {
        SetGlicAccessing(true);

        // This simulates closing the active tab via CloseContents.
        content::WebContents* web_contents =
            browser()->tab_strip_model()->GetWebContentsAt(0);
        BrowserWebContentsDelegate::From(browser())->CloseContents(
            web_contents);
      }),
      InAnyContext(WaitForShow(actor::ActorTaskTabCloseConfirmDialog::kViewId)),
      Check([&]() {
        auto* dialog = GetActorDialogWidget(UnloadController::From(browser()));
        if (!dialog) {
          return false;
        }
        SetGlicAccessing(false);
        dialog->widget_delegate()->AsDialogDelegate()->AcceptDialog();
        return true;
      }),
      Do([&]() { SetGlicAccessing(false); }));
}

// Tests that if the flag is disabled, the dialog does not appear.
IN_PROC_BROWSER_TEST_F(UnloadControllerGlicDisabledInteractiveUiTest,
                       NoDialogWhenFlagDisabled) {
  ASSERT_TRUE(AddTabAtIndex(0, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  glic::GlicKeyedService* service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(
          browser()->GetProfile());
  ASSERT_TRUE(service);
  service->SetContextAccessIndicator(true);

  // Should be allowed to close immediately without dialog.
  EXPECT_FALSE(
      UnloadController::From(browser())->RunUnloadListenerBeforeClosing(
          web_contents));
}

}  // namespace
