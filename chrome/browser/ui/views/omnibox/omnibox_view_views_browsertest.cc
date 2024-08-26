// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"

#include <stddef.h>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ssl/typed_navigation_upgrade_throttle.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/security_interstitials/core/omnibox_https_upgrade_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/mock_input_method.h"
#include "ui/base/ime/text_edit_commands.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display_switches.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/accessibility/atomic_view_ax_tree_manager.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/textfield/textfield_test_api.h"
#include "ui/views/views_features.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_safearray.h"
#include "chrome/browser/ui/views/accessibility/uia_accessibility_event_waiter.h"
#include "ui/display/win/screen_win.h"
#include "ui/views/win/hwnd_util.h"
#endif

namespace {

void SetClipboardText(ui::ClipboardBuffer buffer, const std::u16string& text) {
  ui::ScopedClipboardWriter(buffer).WriteText(text);
}

}  // namespace

class OmniboxViewViewsTest : public InProcessBrowserTest {
 public:
  OmniboxViewViewsTest(const OmniboxViewViewsTest&) = delete;
  OmniboxViewViewsTest& operator=(const OmniboxViewViewsTest&) = delete;

 protected:
  OmniboxViewViewsTest() = default;
  ~OmniboxViewViewsTest() override = default;

  OmniboxTriggeredFeatureService* triggered_feature_service() {
    return &triggered_feature_service_;
  }

  static void GetOmniboxViewForBrowser(const Browser* browser,
                                       OmniboxView** omnibox_view) {
    BrowserWindow* window = browser->window();
    ASSERT_TRUE(window);
    LocationBar* location_bar = window->GetLocationBar();
    ASSERT_TRUE(location_bar);
    *omnibox_view = location_bar->GetOmniboxView();
    ASSERT_TRUE(*omnibox_view);
  }

  // Move the mouse to the center of the browser window and left-click.
  void ClickBrowserWindowCenter() {
    ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(
        BrowserView::GetBrowserViewForBrowser(
            browser())->GetBoundsInScreen().CenterPoint()));
    ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(ui_controls::LEFT,
                                                   ui_controls::DOWN));
    ASSERT_TRUE(
        ui_test_utils::SendMouseEventsSync(ui_controls::LEFT, ui_controls::UP));
  }

  // Press and release the mouse at the specified locations.  If
  // |release_offset| differs from |press_offset|, the mouse will be moved
  // between the press and release.
  void Click(ui_controls::MouseButton button,
             const gfx::Point& press_location,
             const gfx::Point& release_location) {
    ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(press_location));
    ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(button, ui_controls::DOWN));

    if (press_location != release_location)
      ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(release_location));
    ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(button, ui_controls::UP));
  }

  // Touch down and release at the specified locations.
  void Tap(const gfx::Point& press_location,
           const gfx::Point& release_location) {
    gfx::NativeWindow window = GetRootWindow();
    ui::test::EventGenerator generator(window);
    if (press_location == release_location) {
      generator.GestureTapAt(press_location);
    } else {
      generator.GestureScrollSequence(press_location, release_location,
                                      base::Milliseconds(10), 1);
    }
  }

  OmniboxView* omnibox() {
    return browser()->window()->GetLocationBar()->GetOmniboxView();
  }

  void PressEnterAndWaitForNavigations(size_t num_expected_navigations) {
    content::TestNavigationObserver navigation_observer(
        browser()->tab_strip_model()->GetActiveWebContents(),
        num_expected_navigations);
    EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_RETURN,
                                                false, false, false, false));
    navigation_observer.Wait();
  }

 private:
  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Some of these tests assume a 1.0 device scale factor.
    command_line->AppendSwitchASCII(::switches::kForceDeviceScaleFactor, "1");
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
    chrome::FocusLocationBar(browser());
    ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  }

  gfx::NativeWindow GetRootWindow() const {
    gfx::NativeWindow native_window = browser()->window()->GetNativeWindow();
#if defined(USE_AURA)
    native_window = native_window->GetRootWindow();
#endif
    return native_window;
  }

  OmniboxTriggeredFeatureService triggered_feature_service_;
};

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, PasteAndGoDoesNotLeavePopupOpen) {
  OmniboxView* view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &view));
  OmniboxViewViews* omnibox_view_views = static_cast<OmniboxViewViews*>(view);

  // Put a URL on the clipboard.
  SetClipboardText(ui::ClipboardBuffer::kCopyPaste, u"http://www.example.com/");

  // Paste and go.
  omnibox_view_views->ExecuteCommand(IDC_PASTE_AND_GO, ui::EF_NONE);

  // The popup should not be open.
  EXPECT_FALSE(view->model()->PopupIsOpen());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, DoNotNavigateOnDrop) {
  OmniboxView* view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &view));
  OmniboxViewViews* omnibox_view_views = static_cast<OmniboxViewViews*>(view);

  OSExchangeData data;
  std::u16string input = u"Foo bar baz";
  EXPECT_FALSE(data.HasString());
  data.SetString(input);
  EXPECT_TRUE(data.HasString());

  ui::DropTargetEvent event(data, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_COPY);
  omnibox_view_views->OnDrop(event);
  EXPECT_EQ(input, omnibox_view_views->GetText());
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(omnibox_view_views->IsSelectAll());
  EXPECT_FALSE(
      browser()->tab_strip_model()->GetActiveWebContents()->IsLoading());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, AyncDropCallback) {
  OmniboxView* view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &view));
  OmniboxViewViews* omnibox_view_views = static_cast<OmniboxViewViews*>(view);

  OSExchangeData data;
  std::u16string input = u"Foo bar baz";
  EXPECT_FALSE(data.HasString());
  data.SetString(input);
  EXPECT_TRUE(data.HasString());

  ui::DropTargetEvent event(data, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_COPY);
  auto cb = omnibox_view_views->CreateDropCallback(event);
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(cb).Run(event, output_drag_op, /*drag_image_layer_owner=*/nullptr);

  EXPECT_EQ(input, omnibox_view_views->GetText());
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(omnibox_view_views->IsSelectAll());
  EXPECT_FALSE(
      browser()->tab_strip_model()->GetActiveWebContents()->IsLoading());
}

// Flaky: https://crbug.com/915591.
IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, DISABLED_SelectAllOnClick) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  omnibox_view->SetUserText(u"http://www.google.com/");

  // Take the focus away from the omnibox.
  ASSERT_NO_FATAL_FAILURE(ClickBrowserWindowCenter());
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Clicking in the omnibox should take focus and select all text.
  const gfx::Rect omnibox_bounds = BrowserView::GetBrowserViewForBrowser(
        browser())->GetViewByID(VIEW_ID_OMNIBOX)->GetBoundsInScreen();
  const gfx::Point click_location = omnibox_bounds.CenterPoint();
  ASSERT_NO_FATAL_FAILURE(Click(ui_controls::LEFT,
                                click_location, click_location));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Clicking in another view should clear focus and the selection.
  ASSERT_NO_FATAL_FAILURE(ClickBrowserWindowCenter());
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Clicking in the omnibox again should take focus and select all text again.
  ASSERT_NO_FATAL_FAILURE(Click(ui_controls::LEFT,
                                click_location, click_location));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Clicking another omnibox spot should keep focus but clear the selection.
  omnibox_view->SelectAll(false);
  const gfx::Point click2_location = omnibox_bounds.origin() +
      gfx::Vector2d(omnibox_bounds.width() / 4, omnibox_bounds.height() / 4);
  ASSERT_NO_FATAL_FAILURE(Click(ui_controls::LEFT,
                                click2_location, click2_location));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Take the focus away and click in the omnibox again, but drag a bit before
  // releasing.  We should focus the omnibox but not select all of its text.
  ASSERT_NO_FATAL_FAILURE(ClickBrowserWindowCenter());
  ASSERT_NO_FATAL_FAILURE(Click(ui_controls::LEFT,
                                click_location, click2_location));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Middle-click is only handled on Linux, by pasting the selection clipboard
  // and moving the cursor after the pasted text instead of selecting-all.
  ASSERT_NO_FATAL_FAILURE(ClickBrowserWindowCenter());
  ASSERT_NO_FATAL_FAILURE(Click(ui_controls::MIDDLE,
                                click_location, click_location));
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
#else
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
#endif
  EXPECT_FALSE(omnibox_view->IsSelectAll());
}

// TODO(crbug.com/339098004): Flaky across multiple builders.
IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, DISABLED_SelectionClipboard) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  omnibox_view->SetUserText(u"http://www.google.com/");
  OmniboxViewViews* omnibox_view_views =
      static_cast<OmniboxViewViews*>(omnibox_view);
  ASSERT_TRUE(omnibox_view_views);
  gfx::RenderText* render_text = omnibox_view_views->GetRenderText();

  // Take the focus away from the omnibox.
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  size_t cursor_position = 14;
  int cursor_x = render_text->GetCursorBounds(
      gfx::SelectionModel(cursor_position, gfx::CURSOR_FORWARD), false).x();
  gfx::Point click_location = omnibox_view_views->GetBoundsInScreen().origin();
  click_location.Offset(cursor_x + render_text->display_rect().x(),
                        omnibox_view_views->height() / 2);

  // Middle click focuses the omnibox, pastes, and sets a trailing cursor.
  // Select-all on focus shouldn't alter the selection clipboard or cursor.
  SetClipboardText(ui::ClipboardBuffer::kSelection, u"123");
  ASSERT_NO_FATAL_FAILURE(Click(ui_controls::MIDDLE,
                                click_location, click_location));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());
  EXPECT_EQ(u"http://www.goo123gle.com/", omnibox_view->GetText());
  EXPECT_EQ(17U, omnibox_view_views->GetCursorPosition());

  // The omnibox can move when it gains focus, so refresh the click location.
  click_location.set_x(omnibox_view_views->GetBoundsInScreen().origin().x() +
                       cursor_x + render_text->display_rect().x());

  // Middle clicking again, with focus, pastes and updates the cursor.
  SetClipboardText(ui::ClipboardBuffer::kSelection, u"4567");
  ASSERT_NO_FATAL_FAILURE(Click(ui_controls::MIDDLE,
                                click_location, click_location));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());
  EXPECT_EQ(u"http://www.goo4567123gle.com/", omnibox_view->GetText());
  EXPECT_EQ(18U, omnibox_view_views->GetCursorPosition());
}

// No touch on desktop Mac. Tracked in http://crbug.com/445520.
#if !BUILDFLAG(IS_MAC) || defined(USE_AURA)

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, SelectAllOnTap) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  omnibox_view->SetUserText(u"http://www.google.com/");

  // Take the focus away from the omnibox.
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Tapping in the omnibox should take focus and select all text.
  const gfx::Rect omnibox_bounds = BrowserView::GetBrowserViewForBrowser(
      browser())->GetViewByID(VIEW_ID_OMNIBOX)->GetBoundsInScreen();
  const gfx::Point tap_location = omnibox_bounds.CenterPoint();
  ASSERT_NO_FATAL_FAILURE(Tap(tap_location, tap_location));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Tapping in another view should clear focus and the selection.
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Tapping in the omnibox again should take focus and select all text again.
  ASSERT_NO_FATAL_FAILURE(Tap(tap_location, tap_location));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Tapping another omnibox spot should keep focus and selection.
  omnibox_view->SelectAll(false);
  const gfx::Point tap2_location = omnibox_bounds.origin() +
      gfx::Vector2d(omnibox_bounds.width() / 4, omnibox_bounds.height() / 4);
  ASSERT_NO_FATAL_FAILURE(Tap(tap2_location, tap2_location));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  // We don't test if the all text is selected because it depends on whether
  // there was text under the tap, which appears to be flaky.

  // Take the focus away and tap in the omnibox again, but drag a bit before
  // releasing. This shouldn't select text.
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER));
  ASSERT_NO_FATAL_FAILURE(Tap(tap_location, tap2_location));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());
}

// Tests if executing a command hides touch editing handles.
IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest,
                       DeactivateTouchEditingOnExecuteCommand) {
  OmniboxView* view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &view));
  OmniboxViewViews* omnibox_view_views = static_cast<OmniboxViewViews*>(view);
  views::TextfieldTestApi textfield_test_api(omnibox_view_views);

  // Put a URL on the clipboard.
  SetClipboardText(ui::ClipboardBuffer::kCopyPaste, u"http://www.example.com/");

  // Tap to activate touch editing.
  gfx::Point omnibox_center =
      omnibox_view_views->GetBoundsInScreen().CenterPoint();
  Tap(omnibox_center, omnibox_center);
  EXPECT_TRUE(textfield_test_api.touch_selection_controller());

  // Execute a command and check if it deactivates touch editing. Paste & Go is
  // chosen since it is specific to Omnibox and its execution wouldn't be
  // delegated to the base Textfield class.
  omnibox_view_views->ExecuteCommand(IDC_PASTE_AND_GO, ui::EF_NONE);
  EXPECT_FALSE(textfield_test_api.touch_selection_controller());
}

#endif  // !BUILDFLAG(IS_MAC) || defined(USE_AURA)

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, SelectAllOnTabToFocus) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("http://www.google.com/")));
  // RevertAll after navigation to invalidate the selection range saved on blur.
  omnibox_view->RevertAll();
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Pressing tab to focus the omnibox should select all text.
  while (!ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX)) {
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB,
                                                false, false, false, false));
  }
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(omnibox_view->IsSelectAll());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, CloseOmniboxPopupOnTextDrag) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  OmniboxViewViews* omnibox_view_views =
      static_cast<OmniboxViewViews*>(omnibox_view);

  // Populate suggestions for the omnibox popup.
  AutocompleteController* autocomplete_controller =
      omnibox_view->controller()->autocomplete_controller();
  AutocompleteResult& results = autocomplete_controller->internal_result_;
  ACMatches matches;
  AutocompleteMatch match(nullptr, 500, false,
                          AutocompleteMatchType::HISTORY_TITLE);
  match.contents = u"http://autocomplete-result/";
  match.contents_class.push_back(
      ACMatchClassification(0, ACMatchClassification::URL));
  match.destination_url = GURL("http://autocomplete-result/");
  match.allowed_to_be_default_match = true;
  matches.push_back(match);
  AutocompleteInput input(u"a", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  results.AppendMatches(matches);
  results.SortAndCull(
      input, TemplateURLServiceFactory::GetForProfile(browser()->profile()),
      triggered_feature_service());

  // The omnibox popup should open with suggestions displayed.
  autocomplete_controller->NotifyChanged();
  EXPECT_TRUE(omnibox_view->model()->PopupIsOpen());

  // The omnibox text should be selected.
  EXPECT_TRUE(omnibox_view->IsSelectAll());

  // Simulate a mouse click before dragging the mouse.
  gfx::Point point(omnibox_view_views->origin() + gfx::Vector2d(10, 10));
  ui::MouseEvent pressed(ui::EventType::kMousePressed, point, point,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  omnibox_view_views->OnMousePressed(pressed);
  EXPECT_TRUE(omnibox_view->model()->PopupIsOpen());

  // Simulate a mouse drag of the omnibox text, and the omnibox should close.
  ui::MouseEvent dragged(ui::EventType::kMouseDragged, point, point,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
  omnibox_view_views->OnMouseDragged(dragged);

  EXPECT_FALSE(omnibox_view->model()->PopupIsOpen());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, MaintainCursorAfterFocusCycle) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));

  // Populate suggestions for the omnibox popup.
  AutocompleteController* autocomplete_controller =
      omnibox_view->controller()->autocomplete_controller();
  AutocompleteResult& results = autocomplete_controller->internal_result_;
  ACMatches matches;
  AutocompleteMatch match(nullptr, 500, false,
                          AutocompleteMatchType::HISTORY_TITLE);
  match.contents = u"http://autocomplete-result/";
  match.contents_class.push_back(
      ACMatchClassification(0, ACMatchClassification::URL));
  match.destination_url = GURL("http://autocomplete-result/");
  match.allowed_to_be_default_match = true;
  matches.push_back(match);
  AutocompleteInput input(u"autocomplete-result", 19, "autocomplete-result",
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_current_url(GURL("http://autocomplete-result/"));
  results.AppendMatches(matches);
  results.SortAndCull(
      input, TemplateURLServiceFactory::GetForProfile(browser()->profile()),
      triggered_feature_service());

  // The omnibox popup should open with suggestions displayed.
  autocomplete_controller->NotifyChanged();
  EXPECT_TRUE(omnibox_view->model()->PopupIsOpen());

  // TODO(krb): For some reason, we need to hit End twice to be registered.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_END, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_END, false,
                                              false, false, false));
  EXPECT_FALSE(omnibox_view->IsSelectAll());

  // Save cursor position, before blur.
  size_t prev_start, end;
  omnibox_view->GetSelectionBounds(&prev_start, &end);

  chrome::FocusAppMenu(browser());
  EXPECT_FALSE(omnibox_view->model()->PopupIsOpen());

  // Re-focus.
  chrome::FocusLocationBar(browser());

  // Make sure cursor is restored.
  size_t start;
  omnibox_view->GetSelectionBounds(&start, &end);
  EXPECT_EQ(prev_start, start);
}

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, BackgroundIsOpaque) {
  // The omnibox text should be rendered on an opaque background. Otherwise, we
  // can't use subpixel rendering.
  OmniboxViewViews* view = BrowserView::GetBrowserViewForBrowser(browser())->
      toolbar()->location_bar()->omnibox_view();
  ASSERT_TRUE(view);
  EXPECT_FALSE(view->GetRenderText()->subpixel_rendering_suppressed());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, FocusedTextInputClient) {
  chrome::FocusLocationBar(browser());
  OmniboxView* view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &view));
  OmniboxViewViews* omnibox_view_views = static_cast<OmniboxViewViews*>(view);
  ui::InputMethod* input_method =
      omnibox_view_views->GetWidget()->GetInputMethod();
  EXPECT_EQ(static_cast<ui::TextInputClient*>(omnibox_view_views),
            input_method->GetTextInputClient());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, TextElideStatus) {
  OmniboxView* view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &view));
  OmniboxViewViews* omnibox_view_views = static_cast<OmniboxViewViews*>(view);

  ASSERT_NO_FATAL_FAILURE(ClickBrowserWindowCenter());
  EXPECT_EQ(omnibox_view_views->GetRenderText()->elide_behavior(),
            gfx::ELIDE_TAIL);

  chrome::FocusLocationBar(browser());
  EXPECT_EQ(omnibox_view_views->GetRenderText()->elide_behavior(),
            gfx::NO_ELIDE);
}

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, FragmentUnescapedForDisplay) {
  OmniboxView* view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &view));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("http://example.com/#%E2%98%83")));

  EXPECT_EQ(view->GetText(), u"example.com/#\u2603");
}

// Ensure that when the user navigates between suggestions, that the accessible
// value of the Omnibox includes helpful information for human comprehension,
// such as the document title. When the user begins to arrow left/right
// within the label or edit it, the value is presented as the pure editable URL.
IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, FriendlyAccessibleLabel) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  std::u16string match_url = u"https://google.com";
  AutocompleteMatch match(nullptr, 500, false,
                          AutocompleteMatchType::HISTORY_TITLE);
  match.contents = match_url;
  match.contents_class.push_back(
      ACMatchClassification(0, ACMatchClassification::URL));
  match.destination_url = GURL(match_url);
  match.description = u"Google";
  match.description_class = {{0, 0}};
  match.allowed_to_be_default_match = true;

  // Enter user input mode to prevent spurious unelision.
  omnibox_view->model()->SetInputInProgress(true);

  // Populate suggestions for the omnibox popup.
  AutocompleteController* autocomplete_controller =
      omnibox_view->controller()->autocomplete_controller();
  AutocompleteResult& results = autocomplete_controller->internal_result_;
  ACMatches matches;
  matches.push_back(match);
  AutocompleteInput input(u"g", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  results.AppendMatches(matches);
  results.SortAndCull(
      input, TemplateURLServiceFactory::GetForProfile(browser()->profile()),
      triggered_feature_service());

  // The omnibox popup should open with suggestions displayed.
  chrome::FocusLocationBar(browser());
  autocomplete_controller->NotifyChanged();
  EXPECT_TRUE(omnibox_view->model()->PopupIsOpen());
  OmniboxViewViews* omnibox_view_views =
      static_cast<OmniboxViewViews*>(omnibox_view);

  // Ensure that the friendly accessibility label is updated.
  omnibox_view_views->OnTemporaryTextMaybeChanged(match_url, match, false,
                                                  false);
  omnibox_view->SelectAll(true);
  EXPECT_EQ(u"https://google.com", omnibox_view_views->GetText());

  // Test friendly label.
  const int kFriendlyPrefixLength = match.description.size() + 1;
  ui::AXNodeData node_data;
  omnibox_view_views->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(u"Google https://google.com location from history, 1 of 1",
            node_data.GetString16Attribute(ax::mojom::StringAttribute::kValue));
  // Selection offsets are moved over by length the inserted descriptive text
  // prefix ("Google") + 1 for the space.
  EXPECT_EQ(kFriendlyPrefixLength,
            node_data.GetIntAttribute(ax::mojom::IntAttribute::kTextSelEnd));
  EXPECT_EQ(kFriendlyPrefixLength + static_cast<int>(match_url.size()),
            node_data.GetIntAttribute(ax::mojom::IntAttribute::kTextSelStart));
  EXPECT_EQ("both", node_data.GetStringAttribute(
                        ax::mojom::StringAttribute::kAutoComplete));
  EXPECT_EQ(ax::mojom::Role::kTextField, node_data.role);

  // Select second character -- even though the friendly "Google " prefix is
  // part of the exposed accessible text, setting the selection within select
  // the intended part of the editable text.
  ui::AXActionData set_selection_action_data;
  set_selection_action_data.action = ax::mojom::Action::kSetSelection;
  set_selection_action_data.anchor_node_id = node_data.id;
  set_selection_action_data.focus_node_id = node_data.id;
  set_selection_action_data.focus_offset = kFriendlyPrefixLength + 1;
  set_selection_action_data.anchor_offset = kFriendlyPrefixLength + 3;
  omnibox_view_views->HandleAccessibleAction(set_selection_action_data);

  EXPECT_EQ(u"https://google.com", omnibox_view_views->GetText());

  // Type "x" to replace the selected "tt" with that character.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_X, false,
                                              false, false, false));
  // Second character should be an "x" now.
  EXPECT_EQ(u"hxps://google.com", omnibox_view_views->GetText());

  // When editing starts, the accessible value becomes the same as the raw
  // edited text.
  node_data = ui::AXNodeData();
  omnibox_view_views->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(u"hxps://google.com",
            node_data.GetString16Attribute(ax::mojom::StringAttribute::kValue));
}

// Ensure that the Omnibox popup exposes appropriate accessibility semantics,
// given a current state of open or closed.
IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, AccessiblePopup) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  OmniboxViewViews* omnibox_view_views =
      static_cast<OmniboxViewViews*>(omnibox_view);
  chrome::FocusLocationBar(browser());

  std::u16string match_url = u"https://google.com";
  AutocompleteMatch match(nullptr, 500, false,
                          AutocompleteMatchType::HISTORY_TITLE);
  match.contents = match_url;
  match.contents_class.push_back(
      ACMatchClassification(0, ACMatchClassification::URL));
  match.destination_url = GURL(match_url);
  match.description = u"Google";
  match.allowed_to_be_default_match = true;

  OmniboxPopupView* popup_view = omnibox_view_views->GetPopupViewForTesting();
  ui::AXNodeData popup_node_data_1;
  popup_view->GetPopupAccessibleNodeData(&popup_node_data_1);
  EXPECT_FALSE(popup_node_data_1.HasState(ax::mojom::State::kExpanded));
  EXPECT_TRUE(popup_node_data_1.HasState(ax::mojom::State::kCollapsed));
  EXPECT_TRUE(popup_node_data_1.HasState(ax::mojom::State::kInvisible));

  EXPECT_TRUE(
      popup_node_data_1.HasIntAttribute(ax::mojom::IntAttribute::kPopupForId));
  EXPECT_EQ(
      popup_node_data_1.GetIntAttribute(ax::mojom::IntAttribute::kPopupForId),
      omnibox_view_views->GetViewAccessibility().GetUniqueId());

  // Populate suggestions for the omnibox popup.
  AutocompleteController* autocomplete_controller =
      omnibox_view->controller()->autocomplete_controller();
  AutocompleteResult& results = autocomplete_controller->internal_result_;
  ACMatches matches;
  matches.push_back(match);
  AutocompleteInput input(u"g", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  results.AppendMatches(matches);
  results.SortAndCull(
      input, TemplateURLServiceFactory::GetForProfile(browser()->profile()),
      triggered_feature_service());

  // The omnibox popup should open with suggestions displayed.
  autocomplete_controller->NotifyChanged();
  EXPECT_TRUE(omnibox_view->model()->PopupIsOpen());
  ui::AXNodeData popup_node_data_2;
  popup_view->GetPopupAccessibleNodeData(&popup_node_data_2);
  EXPECT_TRUE(popup_node_data_2.HasState(ax::mojom::State::kExpanded));
  EXPECT_FALSE(popup_node_data_2.HasState(ax::mojom::State::kCollapsed));
  EXPECT_FALSE(popup_node_data_2.HasState(ax::mojom::State::kInvisible));
}

// Flaky: https://crbug.com/1143630.
// Omnibox returns to clean state after chrome://kill and reload.
// https://crbug.com/993701 left the URL and icon as chrome://kill after reload.
IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, DISABLED_ReloadAfterKill) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  OmniboxViewViews* omnibox_view_views =
      static_cast<OmniboxViewViews*>(omnibox_view);

  // Open new tab page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Kill the tab with chrome://kill
  {
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(blink::kChromeUIKillURL)));
    EXPECT_TRUE(tab->IsCrashed());
  }

  // Reload the tab.
  tab->GetController().Reload(content::ReloadType::NORMAL, false);
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // Verify the omnibox contents, URL and icon.
  EXPECT_EQ(u"", omnibox_view_views->GetText());
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            browser()->location_bar_model()->GetURL());
}

// Omnibox un-elides and elides URL appropriately according to the Always Show
// Full URLs setting.
IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, AlwaysShowFullURLs) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  OmniboxViewViews* omnibox_view_views =
      static_cast<OmniboxViewViews*>(omnibox_view);

  ASSERT_TRUE(embedded_test_server()->Start());
  // Use a hostname ("a.test") since IP addresses aren't eligible for eliding.
  GURL url = embedded_test_server()->GetURL("a.test", "/title1.html");
  std::u16string url_text = base::ASCIIToUTF16(url.spec());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  EXPECT_EQ(url_text, u"http://" + omnibox_view_views->GetText());

  // After toggling the setting, the full URL should be shown.
  chrome::ToggleShowFullURLs(browser());
  EXPECT_EQ(url_text, omnibox_view_views->GetText());
  EXPECT_EQ(0,
            omnibox_view_views->GetRenderText()->GetUpdatedDisplayOffset().x());

  // Toggling the setting again should go back to the elided URL.
  chrome::ToggleShowFullURLs(browser());
  EXPECT_EQ(url_text, u"http://" + omnibox_view_views->GetText());
}

// The following set of tests require UIA accessibility support, which only
// exists on Windows.
#if BUILDFLAG(IS_WIN)
using Microsoft::WRL::ComPtr;

class OmniboxViewViewsUIATest : public OmniboxViewViewsTest {
 public:
  OmniboxViewViewsUIATest() {}

  void ExpectUIADoubleSafearrayEQ(
      SAFEARRAY* safearray,
      const std::vector<double>& expected_property_values) {
    EXPECT_EQ(sizeof(V_R8(LPVARIANT(NULL))), ::SafeArrayGetElemsize(safearray));
    ASSERT_EQ(1u, SafeArrayGetDim(safearray));
    LONG array_lower_bound;
    ASSERT_HRESULT_SUCCEEDED(
        SafeArrayGetLBound(safearray, 1, &array_lower_bound));
    LONG array_upper_bound;
    ASSERT_HRESULT_SUCCEEDED(
        SafeArrayGetUBound(safearray, 1, &array_upper_bound));
    double* array_data;
    ASSERT_HRESULT_SUCCEEDED(::SafeArrayAccessData(
        safearray, reinterpret_cast<void**>(&array_data)));
    size_t count = array_upper_bound - array_lower_bound + 1;
    ASSERT_EQ(expected_property_values.size(), count);
    for (size_t i = 0; i < count; ++i) {
      EXPECT_EQ(array_data[i], expected_property_values[i]);
    }
    ASSERT_HRESULT_SUCCEEDED(::SafeArrayUnaccessData(safearray));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{::features::kUiaProvider};
};

// Omnibox fires the right events when the popup opens/closes with UIA turned
// on.
IN_PROC_BROWSER_TEST_F(OmniboxViewViewsUIATest, AccessibleOmnibox) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  chrome::FocusLocationBar(browser());

  std::u16string match_url = u"https://example.com";
  AutocompleteMatch match(nullptr, 500, false,
                          AutocompleteMatchType::HISTORY_TITLE);
  match.contents = match_url;
  match.contents_class.push_back(
      ACMatchClassification(0, ACMatchClassification::URL));
  match.destination_url = GURL(match_url);
  match.description = u"Example";
  match.allowed_to_be_default_match = true;

  EXPECT_FALSE(omnibox_view->model()->PopupIsOpen());

  HWND window_handle =
      browser()->window()->GetNativeWindow()->GetHost()->GetAcceleratedWidget();
  UiaAccessibilityWaiterInfo info = {window_handle, L"textbox",
                                     L"Address and search bar",
                                     ax::mojom::Event::kControlsChanged};
  UiaAccessibilityEventWaiter open_waiter(info);

  // Populate suggestions for the omnibox popup.
  AutocompleteController* autocomplete_controller =
      omnibox_view->controller()->autocomplete_controller();
  AutocompleteResult& results = autocomplete_controller->internal_result_;
  ACMatches matches;
  matches.push_back(match);
  AutocompleteInput input(u"e", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  results.AppendMatches(matches);
  results.SortAndCull(
      input, TemplateURLServiceFactory::GetForProfile(browser()->profile()),
      triggered_feature_service());

  // The omnibox popup should open with suggestions displayed.
  autocomplete_controller->NotifyChanged();

  // Wait for ControllerFor property changed event.
  open_waiter.Wait();

  EXPECT_TRUE(omnibox_view->model()->PopupIsOpen());

  UiaAccessibilityEventWaiter close_waiter(info);
  // Close the popup. Another property change event is expected.
  ClickBrowserWindowCenter();
  close_waiter.Wait();
  EXPECT_FALSE(omnibox_view->model()->PopupIsOpen());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsUIATest, GetSelectionAndBounds) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  OmniboxViewViews* omnibox_view_views =
      static_cast<OmniboxViewViews*>(omnibox_view);

  views::AtomicViewAXTreeManager* ax_tree_manager =
      omnibox_view_views->GetViewAccessibility()
          .GetAtomicViewAXTreeManagerForTesting();

  // Set the text and select a substring.
  gfx::Range selection_range(4, 8);

  omnibox_view_views->SetUserText(u"helloworld");
  omnibox_view_views->SetSelectedRange(selection_range);

  // Get the UIA providers we need to perform the tests.
  ui::AXPlatformNode* ax_platform_node =
      ui::AXPlatformNode::FromNativeViewAccessible(
          ax_tree_manager->RootDelegate()->GetNativeViewAccessible());

  ComPtr<IRawElementProviderSimple> root_node_raw;
  ax_platform_node->GetNativeViewAccessible()->QueryInterface(
      __uuidof(IRawElementProviderSimple), &root_node_raw);
  ComPtr<ITextProvider> document_provider;
  ASSERT_HRESULT_SUCCEEDED(
      root_node_raw->GetPatternProvider(UIA_TextPatternId, &document_provider));

  CComPtr<ITextRangeProvider> selected_text_range_provider;
  base::win::ScopedSafearray selection;
  LONG index = 0;

  // The actual testing starts:
  // 1. Call ITextProvider::GetSelection to get the selected text range.
  document_provider->GetSelection(selection.Receive());
  ASSERT_NE(nullptr, selection.Get());
  ASSERT_HRESULT_SUCCEEDED(SafeArrayGetElement(selection.Get(), &index,
                                               &selected_text_range_provider));

  // 2. Call ITextRangeProvider::GetText to get the selected text.
  base::win::ScopedBstr text_content;
  ASSERT_HRESULT_SUCCEEDED(
      selected_text_range_provider->GetText(-1, text_content.Receive()));
  EXPECT_STREQ(L"owor", text_content.Get());

  // 3. Call ITextRangeProvider::GetBoundingRectangles to get the bounding rect.
  base::win::ScopedSafearray safearray;
  ASSERT_HRESULT_SUCCEEDED(
      selected_text_range_provider->GetBoundingRectangles(safearray.Receive()));

  ComPtr<IRawElementProviderFragment> textfield_fragment;
  ax_platform_node->GetNativeViewAccessible()->QueryInterface(
      __uuidof(IRawElementProviderFragment), &textfield_fragment);
  UiaRect textfield_rect;

  ASSERT_HRESULT_SUCCEEDED(
      textfield_fragment->get_BoundingRectangle(&textfield_rect));

  // Create the expected bounds for the text range from the bounds of the text
  // field and the selected range offsets.
  gfx::Rect bounds_in_screen = omnibox_view_views->GetContentsBounds();
  omnibox_view_views->ConvertRectToScreen(omnibox_view_views,
                                          &bounds_in_screen);

  std::vector<int32_t> offsets =
      ax_tree_manager->GetRoot()->GetIntListAttribute(
          ax::mojom::IntListAttribute::kCharacterOffsets);
  int range_start_offset = offsets[selection_range.start()];
  int range_end_offset = offsets[selection_range.end()];

  int left_bound =
      display::win::ScreenWin::DIPToScreenRect(
          HWNDForView(omnibox_view_views),
          gfx::Rect(range_start_offset + bounds_in_screen.x(), 0, 0, 0))
          .x();

  // Adust `textfield_rect` to account for the border.
  textfield_rect.top += omnibox_view_views->GetInsets().top();
  textfield_rect.height -= omnibox_view_views->GetInsets().height();

  std::vector<double> expected_values = std::vector<double>{
      static_cast<double>(left_bound), textfield_rect.top,
      static_cast<double>(range_end_offset - range_start_offset),
      textfield_rect.height};

  ASSERT_NO_FATAL_FAILURE(
      ExpectUIADoubleSafearrayEQ(safearray.Get(), expected_values));
}

namespace {

class OmniboxMockInputMethod : public ui::MockInputMethod {
 public:
  OmniboxMockInputMethod() : ui::MockInputMethod(nullptr) {}
  // ui::MockInputMethod:
  bool IsInputLocaleCJK() const override { return input_locale_cjk_; }
  void SetInputLocaleCJK(bool is_cjk) { input_locale_cjk_ = is_cjk; }

 private:
  bool input_locale_cjk_ = false;
};

}  // namespace

class OmniboxViewViewsIMETest : public OmniboxViewViewsTest {
 public:
  OmniboxViewViewsIMETest() = default;

  // OmniboxViewViewsTest:
  void SetUp() override {
    input_method_ = new OmniboxMockInputMethod();
    // Transfers ownership.
    ui::SetUpInputMethodForTesting(input_method_);
    InProcessBrowserTest::SetUp();
  }

 protected:
  OmniboxMockInputMethod* GetInputMethod() const { return input_method_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{::features::kUiaProvider};
  raw_ptr<OmniboxMockInputMethod, AcrossTasksDanglingUntriaged> input_method_ =
      nullptr;
};

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsIMETest, TextInputTypeChangedTest) {
  chrome::FocusLocationBar(browser());
  OmniboxView* view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &view));
  OmniboxViewViews* omnibox_view_views = static_cast<OmniboxViewViews*>(view);
  ui::InputMethod* input_method =
      omnibox_view_views->GetWidget()->GetInputMethod();
  EXPECT_EQ(input_method, GetInputMethod());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_URL, omnibox_view_views->GetTextInputType());
  GetInputMethod()->SetInputLocaleCJK(/*is_cjk=*/true);
  omnibox_view_views->OnInputMethodChanged();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_SEARCH, omnibox_view_views->GetTextInputType());

  GetInputMethod()->SetInputLocaleCJK(/*is_cjk=*/false);
  omnibox_view_views->OnInputMethodChanged();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_URL, omnibox_view_views->GetTextInputType());
}

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsIMETest, TextInputTypeInitRespectsIME) {
  OmniboxMockInputMethod* input_method = new OmniboxMockInputMethod();
  ui::SetUpInputMethodForTesting(input_method);
  input_method->SetInputLocaleCJK(/*is_cjk=*/true);
  Browser* browser_2 = CreateBrowser(browser()->profile());
  OmniboxView* view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser_2, &view));
  OmniboxViewViews* omnibox_view_views = static_cast<OmniboxViewViews*>(view);
  EXPECT_EQ(omnibox_view_views->GetWidget()->GetInputMethod(), input_method);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_SEARCH, omnibox_view_views->GetTextInputType());
  input_method->SetInputLocaleCJK(/*is_cjk=*/false);
  omnibox_view_views->OnInputMethodChanged();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_URL, omnibox_view_views->GetTextInputType());
}
#endif  // BUILDFLAG(IS_WIN)

// ClickOnView(VIEW_ID_OMNIBOX) does not set focus to omnibox on Mac.
// Looks like the same problem as in the SelectAllOnClick().
// Tracked in: https://crbug.com/915591
// Test is also flaky on Linux: https://crbug.com/1157250
// Click goes to the popup widget, but doesn't sets focus to the popup view if
// it's a separate accelerated widget on Lacros: https://crbug.com/329271186
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_HandleExternalProtocolURLs DISABLED_HandleExternalProtocolURLs
#else
#define MAYBE_HandleExternalProtocolURLs HandleExternalProtocolURLs
#endif

// Checks that focusing on the omnibox allows the page to open external protocol
// URLs. Regression test for https://crbug.com/1143632
IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, MAYBE_HandleExternalProtocolURLs) {
  OmniboxView* omnibox_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetOmniboxViewForBrowser(browser(), &omnibox_view));
  AutocompleteController* controller =
      omnibox_view->controller()->autocomplete_controller();
  ASSERT_TRUE(controller);

  auto set_text_and_perform_navigation = [this, omnibox_view, controller]() {
    const char fake_protocol[] = "fake";
    const char16_t fake_url[] = u"fake://path";

    EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));

    // Set omnibox text and wait for autocomplete.
    omnibox_view->SetUserText(fake_url);
    if (!controller->done())
      ui_test_utils::WaitForAutocompleteDone(browser());
    ASSERT_TRUE(controller->done());
    ASSERT_TRUE(omnibox_view->model()->PopupIsOpen());

    EXPECT_NE(ExternalProtocolHandler::BLOCK,
              ExternalProtocolHandler::GetBlockState(fake_protocol, nullptr,
                                                     browser()->profile()));

    // Check SWYT and UWYT suggestions
    const AutocompleteResult& result = controller->result();
    ASSERT_EQ(result.size(), 2U);
    EXPECT_EQ(result.match_at(0).type,
              AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED);
    EXPECT_EQ(result.match_at(1).type,
              AutocompleteMatchType::URL_WHAT_YOU_TYPED);

    // Navigate to UWYT suggestion.
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_DOWN, false,
                                                false, false, false));
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_RETURN,
                                                false, false, false, false));

    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(content::WaitForLoadStop(tab));

    EXPECT_EQ(ExternalProtocolHandler::BLOCK,
              ExternalProtocolHandler::GetBlockState(fake_protocol, nullptr,
                                                     browser()->profile()));
  };

  set_text_and_perform_navigation();

  // Set focus to omnibox by click.
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::ClickOnView(browser(), VIEW_ID_OMNIBOX));
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::WaitForViewFocus(browser(), VIEW_ID_OMNIBOX, true));

  set_text_and_perform_navigation();

// No touch on desktop Mac. Tracked in http://crbug.com/445520.
#if !BUILDFLAG(IS_MAC) || defined(USE_AURA)

  // Set focus to omnibox by tap.
  const gfx::Point omnibox_tap_point =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->GetViewByID(VIEW_ID_OMNIBOX)
          ->GetBoundsInScreen()
          .CenterPoint();
  ASSERT_NO_FATAL_FAILURE(Tap(omnibox_tap_point, omnibox_tap_point));
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::WaitForViewFocus(browser(), VIEW_ID_OMNIBOX, true));

  set_text_and_perform_navigation();

#endif  // !BUILDFLAG(IS_MAC) || defined(USE_AURA)
}

// SendKeyPressSync times out on Mac, probably due to https://crbug.com/824418.
// TODO(crbug.com/332299695): Also fails on lacros.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_DefaultTypedNavigationsToHttps_ZeroSuggest_NoUpgrade \
  DISABLED_DefaultTypedNavigationsToHttps_ZeroSuggest_NoUpgrade
#else
#define MAYBE_DefaultTypedNavigationsToHttps_ZeroSuggest_NoUpgrade \
  DefaultTypedNavigationsToHttps_ZeroSuggest_NoUpgrade
#endif

// Test that triggers a zero suggest autocomplete request by clicking on the
// omnibox. These should never attempt an HTTPS upgrade or involve the typed
// navigation upgrade throttle.
// This is a regression test for https://crbug.com/1251065
IN_PROC_BROWSER_TEST_F(
    OmniboxViewViewsTest,
    MAYBE_DefaultTypedNavigationsToHttps_ZeroSuggest_NoUpgrade) {
  // Since the embedded test server only works for URLs with non-default ports,
  // use a URLLoaderInterceptor to mimic port-free operation. This allows the
  // rest of the test to operate as if all URLs are using the default ports.
  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.host() == "site-with-good-https.com") {
          std::string headers =
              "HTTP/1.1 200 OK\nContent-Type: text/html; charset=utf-8\n";
          std::string body = "<html><title>Success</title>Hello world</html>";
          content::URLLoaderInterceptor::WriteResponse(headers, body,
                                                       params->client.get());
          return true;
        }
        // Not handled by us.
        return false;
      }));

  base::HistogramTester histograms;
  const GURL url("https://site-with-good-https.com");

  // Type "https://site-with-good-https.com". This should load fine without
  // hitting TypedNavigationUpgradeThrottle.
  omnibox()->SetUserText(base::UTF8ToUTF16(url.spec()), true);
  PressEnterAndWaitForNavigations(1);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(url, contents->GetLastCommittedURL());
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));

  histograms.ExpectTotalCount(
      security_interstitials::omnibox_https_upgrades::kEventHistogram, 0);
  ui_test_utils::HistoryEnumerator enumerator(browser()->profile());
  EXPECT_TRUE(base::Contains(enumerator.urls(), url));

  // Now click the omnibox. This should trigger a zero suggest request with the
  // text "site-with-good-https.com" despite the omnibox URL being
  // https://site-with-good-https.com. Autocomplete input class shouldn't try
  // to upgrade this request.
  const gfx::Rect omnibox_bounds =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->GetViewByID(VIEW_ID_OMNIBOX)
          ->GetBoundsInScreen();
  const gfx::Point click_location = omnibox_bounds.CenterPoint();
  ASSERT_NO_FATAL_FAILURE(
      Click(ui_controls::LEFT, click_location, click_location));
  PressEnterAndWaitForNavigations(1);
  histograms.ExpectTotalCount(
      security_interstitials::omnibox_https_upgrades::kEventHistogram, 0);
}
