// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view_class_properties.h"

class WebUIToolbarWebViewInteractiveTest : public InteractiveBrowserTest {
 public:
  WebUIToolbarWebViewInteractiveTest() {
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIReloadButton,
         features::kSkipIPCChannelPausingForNonGuests,
         features::kWebUIInProcessResourceLoadingV2,
         features::kInitialWebUISyncNavStartToCommit},
        {});
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewInteractiveTest, FocusReloadButton) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebUIToolbarWebViewId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInstrumentedWebViewId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                      kShouldDeferShowState);

  RunTestSequence(
      WaitForShow(kWebUIToolbarElementIdentifier),
      WithView(kWebUIToolbarElementIdentifier,
               [kInstrumentedWebViewId](WebUIToolbarWebView* parent) {
                 parent->GetWebViewForTesting()->SetProperty(
                     views::kElementIdentifierKey, kInstrumentedWebViewId);
               }),
      InstrumentNonTabWebView(kWebUIToolbarWebViewId, kInstrumentedWebViewId,
                              /*wait_for_ready=*/true),
      PollStateUntil(
          kShouldDeferShowState,
          [this]() {
            auto* manager = InitialWebUIManager::From(browser());
            CHECK(manager);
            return manager->RequestDeferShow(base::DoNothing());
          },
          false),
      Do([this]() {
        browser()->command_controller()->ExecuteCommand(IDC_FOCUS_TOOLBAR);
      }),
      CheckJsResultAt(kWebUIToolbarWebViewId, DeepQuery{},
                      "() => {"
                      "  let active = document.activeElement;"
                      "  while (active && active.shadowRoot && "
                      "active.shadowRoot.activeElement) {"
                      "    active = active.shadowRoot.activeElement;"
                      "  }"
                      "  return active ? active.ariaLabel : null;"
                      "}",
                      l10n_util::GetStringUTF8(IDS_ACCNAME_RELOAD)));
}
