// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_PROTOCOL_HANDLER_PICKER_INTERACTVE_UITEST_CC_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_PROTOCOL_HANDLER_PICKER_INTERACTVE_UITEST_CC_

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/web_apps/protocol_handler_picker_dialog.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/window_open_disposition.h"

namespace web_app {

namespace {

std::string Tag(uint32_t i) {
  return base::StringPrintf("row_%i", i);
}

}  // namespace

class ProtocolHandlerPickerUITest
    : public InteractiveBrowserTestMixin<IsolatedWebAppBrowserTestHarness> {
 public:
  webapps::AppId InstallIwaAndAllowProtocolLinkHandling() {
    auto app_id = web_app::IsolatedWebAppBuilder(
                      web_app::ManifestBuilder()
                          .SetName("app-1.0.0")
                          .SetVersion("1.0.0")
                          .AddProtocolHandler("meow", "/index.html?params=%s"))
                      .AddHtml("/index.html", "<html></html>")
                      .BuildBundle()
                      ->InstallChecked(profile())
                      .app_id();
    return app_id;
  }

  void NavigateCurrentTabToAboutBlank() {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("about:blank"), WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  auto LaunchProtocolLink(bool in_new_tab) {
    return Do([&, in_new_tab] {
      ASSERT_THAT(
          content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                          base::StringPrintf("window.open('meow://link', '%s')",
                                             in_new_tab ? "_blank" : "_self")),
          content::EvalJsResult::IsOk());
    });
  }

  auto WaitForAppWindow(const webapps::AppId& app_id) {
    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                        kAppWindowOpened);
    return Steps(PollState(kAppWindowOpened,
                           [&, app_id]() {
                             return !!AppBrowserController::FindForWebApp(
                                 *profile(), app_id);
                           }),
                 WaitForState(kAppWindowOpened, true),
                 StopObservingState(kAppWindowOpened));
  }

  auto WaitForAppWindow() {
    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                        kAppWindowOpened);
    return Steps(PollState(kAppWindowOpened,
                           [&]() {
                             return !!AppBrowserController::FindForWebApp(
                                 *profile(), *selected_app_id_);
                           }),
                 WaitForState(kAppWindowOpened, true),
                 StopObservingState(kAppWindowOpened));
  }

  auto WaitForOpenButton() {
    return WaitForShow(kProtocolHandlerPickerDialogOkButtonId);
  }

  auto ClickOpenButton() {
    return PressButton(kProtocolHandlerPickerDialogOkButtonId);
  }

  auto WaitForDialogHide() {
    // The dialog will always have an ok button, and if the ok button is hidden
    // the dialog is hidden as well.
    return WaitForHide(kProtocolHandlerPickerDialogOkButtonId);
  }

  auto RememberProtocolSelection() {
    return PressButton(kProtocolHandlerPickerDialogRememberSelectionCheckboxId);
  }

  auto WaitForProtocolsLoaded() {
    uint32_t num_apps = apps::AppServiceProxyFactory::GetForProfile(profile())
                            ->GetAppIdsForUrl(GURL("meow://"))
                            .size();
    auto steps = Steps(WaitForShow(kProtocolHandlerPickerDialogSelectionId));
    for (uint32_t i = 0; i < num_apps; i++) {
      steps.push_back(
          NameDescendantViewByType<ProtocolHandlerPickerSelectionRowView>(
              kProtocolHandlerPickerDialogSelectionId, Tag(i), i));
    }
    return InSameContext(std::move(steps));
  }

  auto SelectProtocolHandler(uint32_t index) {
    return InSameContext(
        PressButton(Tag(index)),
        WithView(Tag(index), [&](ProtocolHandlerPickerSelectionRowView* view) {
          selected_app_id_ = view->app_id();
        }));
  }

  auto CheckOpenButtonEnabled(bool enabled) {
    return CheckViewProperty(kProtocolHandlerPickerDialogOkButtonId,
                             &views::View::GetEnabled, enabled);
  }

  auto CheckProtocolHandlerSelected(uint32_t index, bool selected) {
    return CheckViewProperty(Tag(index),
                             &ProtocolHandlerPickerSelectionRowView::IsSelected,
                             selected);
  }

  auto CheckProtocolSelectionRemembered() {
    return InSameContext(
        Check([&] { return selected_app_id_.has_value(); }, "No app selected!"),
        Check(
            [&] {
              return apps::AppServiceProxyFactory::GetForProfile(profile())
                         ->PreferredAppsList()
                         .FindPreferredAppForUrl(GURL("meow://")) ==
                     *selected_app_id_;
            },
            "No preference was set!"));
  }

  // Waits will the default browser gets exactly X tabs.
  auto WaitForTabCount(uint32_t count) {
    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<int>,
                                        kTabCountState);
    return Steps(
        PollState(kTabCountState,
                  [&]() { return browser()->tab_strip_model()->count(); }),
        WaitForState(kTabCountState, count),
        StopObservingState(kTabCountState));
  }

 private:
  std::optional<webapps::AppId> selected_app_id_;
  base::test::ScopedFeatureList features_{
      chromeos::features::kWebAppManifestProtocolHandlerSupport};
};

IN_PROC_BROWSER_TEST_F(ProtocolHandlerPickerUITest, AcceptForOneApp) {
  NavigateCurrentTabToAboutBlank();
  webapps::AppId app_id = InstallIwaAndAllowProtocolLinkHandling();

  RunTestSequence(
      // clang-format off
    // There's currently one open tab.
    WaitForTabCount(1),

    // Trigger the flow.
    Do(&ExternalProtocolHandler::PermitLaunchUrl),
    LaunchProtocolLink(/*in_new_tab=*/false),

    // Wait for the dialog to show up in the same tab.
    WaitForOpenButton(),
    CheckOpenButtonEnabled(true),
    WaitForTabCount(1),

    // Apply selection. This hides the dialog and spawns a new app window; the
    // number of tabs in the default browser stays the same.
    ClickOpenButton(),
    WaitForDialogHide(),
    WaitForAppWindow(app_id),
    WaitForTabCount(1)
      // clang-format on
  );
}

IN_PROC_BROWSER_TEST_F(ProtocolHandlerPickerUITest, AcceptForOneAppInNewTab) {
  NavigateCurrentTabToAboutBlank();
  webapps::AppId app_id = InstallIwaAndAllowProtocolLinkHandling();

  RunTestSequence(
      // clang-format off
    // There's currently one open tab.
    WaitForTabCount(1),

    // Trigger the flow.
    Do(&ExternalProtocolHandler::PermitLaunchUrl),
    LaunchProtocolLink(/*in_new_tab=*/true),

    // Wait for the dialog to show up in a new tab.
    WaitForOpenButton(),
    CheckOpenButtonEnabled(true),
    WaitForTabCount(2),

    // Apply selection. This hides the dialog and spawns a new app window; the
    // number of tabs in the default browser must decrease by one.
    ClickOpenButton(),
    WaitForDialogHide(),
    WaitForAppWindow(app_id),
    WaitForTabCount(1)
      // clang-format on
  );
}

IN_PROC_BROWSER_TEST_F(ProtocolHandlerPickerUITest, AcceptForTwoApps) {
  NavigateCurrentTabToAboutBlank();

  // Install two different apps.
  InstallIwaAndAllowProtocolLinkHandling();
  InstallIwaAndAllowProtocolLinkHandling();

  RunTestSequence(
      // clang-format off
    // There's currently one open tab.
    WaitForTabCount(1),

    // Trigger the flow.
    Do(&ExternalProtocolHandler::PermitLaunchUrl),
    LaunchProtocolLink(/*in_new_tab=*/true),

    // Wait for the dialog to show up.
    // Originally the button must be disabled.
    WaitForProtocolsLoaded(),
    WaitForOpenButton(),
    CheckOpenButtonEnabled(false),
    WaitForTabCount(2),

    // Select the first entry; this enables the open button.
    SelectProtocolHandler(0),
    CheckProtocolHandlerSelected(0, true),
    CheckProtocolHandlerSelected(1, false),
    CheckOpenButtonEnabled(true),

    // Change the selection to second entry; this resets selection on the first
    // entry, but keeps the open button enabled.
    SelectProtocolHandler(1),
    CheckProtocolHandlerSelected(0, false),
    CheckProtocolHandlerSelected(1, true),
    CheckOpenButtonEnabled(true),

    // Also apply the "remember" selection.
    RememberProtocolSelection(),

    // Apply selection. This hides the dialog and spawns a new app window; the
    // number of tabs in the default browser must decrease by one.
    ClickOpenButton(),
    WaitForDialogHide(),
    WaitForAppWindow(),
    WaitForTabCount(1),

    // Also ensure that the selection has been persisted.
    CheckProtocolSelectionRemembered()
      // clang-format on
  );
}

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_PROTOCOL_HANDLER_PICKER_INTERACTVE_UITEST_CC_
