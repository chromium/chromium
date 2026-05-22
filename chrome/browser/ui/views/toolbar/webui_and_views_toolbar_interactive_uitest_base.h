// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_AND_VIEWS_TOOLBAR_INTERACTIVE_UITEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_AND_VIEWS_TOOLBAR_INTERACTIVE_UITEST_BASE_H_

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/test/ui_controls.h"

namespace views {
class WebView;
}

class ReloadButton;
class ReloadControl;
class WebUIReloadControl;

// A class with helpers for UI tests that use the WebUI or Views toolbars.
// Detects toolbar features in using features APIs, and behaves accordingly.
class WebUIAndViewsToolbarInteractiveUiTestBase
    : public InteractiveBrowserTest {
 public:
  WebUIAndViewsToolbarInteractiveUiTestBase();
  ~WebUIAndViewsToolbarInteractiveUiTestBase() override;

  // Deep queries to seled the WebUI reload or back forward buttons. Will
  // CHECK() if the button in question is not being handled by WebUI.
  static WebContentsInteractionTestUtil::DeepQuery WebUIReloadButtonDeepQuery();
  static WebContentsInteractionTestUtil::DeepQuery
  WebUIBackForwardButtonDeepQuery();

  // Returns an ElementIdentifier for the initial tab.
  static ui::ElementIdentifier TabId();

  // Returns an ElementIdentifier for the initial window's WebUI toolbar.
  static ui::ElementIdentifier WebUIToolbarId();

  // Returns the WebView for the WebUI toolbar.
  views::WebView* GetWebUIToolbarWebView();

  // Calls InstrumentToolbar() and waits until the toolbar has loaded.
  MultiStep WaitForToolbarLoaded();

  // Returns the active reload control for browser(), which may be a WebUI
  // control or a Views control, depending on which features are enabled.
  ReloadControl& GetReloadControl();

  // Returns the WebUI reload button control. CHECKs if the WebUI reload button
  // is not enabled.
  WebUIReloadControl& GetWebUIReloadButton();

  // Returns the Views reload button control. CHECKs if the WebUI reload button
  // is enabled.
  ReloadButton& GetNonWebUIReloadButton();

  // Set up the ElementIdentifiers return by TabId() and WebUIToolbarId() to
  // refer to the corresponding elements.
  //
  // The returned step must be run before a step using either of those IDs is
  // actually executed, though the identifiers may be retrieved before running
  // the returned step.
  MultiStep InstrumentToolbar();

  // Moves mouse over the reload button and, if the WebUI reload button is
  // enabled, waits for the ":hover" state to be applied to the button, since
  // the WebUI implementation depends on that state, unlike the Views
  // implementation, which queries the current location of the cursor instead.
  //
  // The step returned by InstrumentToolbar() must be invoked before the
  // returned steps are executed.
  MultiStep MoveMouseOverReloadButton();

  // Move cursor off of the reload button, and if using the WebUI reload
  // button, wait for the ":hover" state to be removed. This is useful because
  // hovering over the reload button affects reload button state (e.g.,
  // hovering when load stops will temporarily disable the button, which
  // affects tests). No wait is necessary with the views toolbar button,
  // because it checks the current location of the cursor, rather than relying
  // on a state that may take a little time to update.
  //
  // The step returned by InstrumentToolbar() must be invoked before the
  // returned steps are executed, so the returned step can find the reload
  // button to wait until it has been informed the mouse is not hovering over
  // the reload button.
  //
  // In theory, it doesn't actually matter where the cursor as moved, as long
  // as it's not on the reload but still on top of the browser window (to make
  // sure simulated events are propagated). However, to remove the ":hover"
  // state on Mac, the cursor needs to still be over the WebUI toolbar on that
  // platform, and so on that platform only, the WebUI back/forward button must
  // be enabled in addition to the reload button, if using this method.
  //
  // TODO(crbug.com/503006742): Remove the use of back/forward button on Mac
  // once this is fixed.
  MultiStep MoveMouseOffOfReloadButton();

  // Waits until the reload button is "ready" after a navigation completes -
  // that means the reload icon is displaying, and not in the double-click
  // timeout period. Note that since the reload button is showing, we also
  // know the button isn't disabled, and the enable timer isn't running, since
  // those only happen while showing the stop icon.
  //
  // The step returned by InstrumentToolbar() must be invoked before the
  // returned steps are executed.
  MultiStep WaitForReloadButtonReady();

  // Waits for the reload button to show an enabled stop icon.
  MultiStep WaitForReloadButtonStopIcon();

  // Waits for the reload button to show a disabled stop icon.
  MultiStep WaitForReloadButtonDisabledStopIcon();

  // This combines moving the mouse over the reload button, waiting for it to be
  // ready, and then clicking it. Arguments match those of
  // InteractiveMouseTestApi::ClickMouse().
  MultiStep ClickReloadButton(
      ui_controls::MouseButton button = ui_controls::LEFT,
      bool release = true,
      int modifier_keys = ui_controls::kNoAccelerator);

  // Waits for the reload button's CSS property to have / not have the
  // ":hover" property, depending on `hover`. InstrumentToolbar() must be called
  // before this step is run. Step only makes sense when
  // IsWebUIReloadButtonEnabled() is true. The Views reload button makes system
  // calls to get the location of the cursor, so always gets the most up-to-date
  // position.
  MultiStep WaitForReloadHover(bool hover);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_AND_VIEWS_TOOLBAR_INTERACTIVE_UITEST_BASE_H_
