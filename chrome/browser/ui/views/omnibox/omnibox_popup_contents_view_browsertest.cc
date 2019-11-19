// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "content/public/test/test_utils.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/theme_provider.h"
#include "ui/compositor/layer_animator.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/accessibility/ax_event_observer.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace {

// A View that positions itself over another View to intercept clicks.
class ClickTrackingOverlayView : public views::View {
 public:
  explicit ClickTrackingOverlayView(OmniboxResultView* result) {
    // |result|'s parent is the OmniboxPopupContentsView, which expects that all
    // its children are OmniboxResultViews.  So skip over it and add this to the
    // OmniboxPopupContentsView's parent.
    auto* contents = result->parent();
    SetBoundsRect(contents->ConvertRectToParent(result->bounds()));
    contents->parent()->AddChildView(this);
  }

  // views::View:
  void OnMouseEvent(ui::MouseEvent* event) override {
    last_click_ = event->location();
  }

  base::Optional<gfx::Point> last_click() const { return last_click_; }

 private:
  base::Optional<gfx::Point> last_click_;
};

// Helper to wait for theme changes. The wait is triggered when an instance of
// this class goes out of scope.
class ThemeChangeWaiter {
 public:
  explicit ThemeChangeWaiter(ThemeService* theme_service)
      : theme_change_observer_(chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                               content::Source<ThemeService>(theme_service)) {}

  ~ThemeChangeWaiter() {
    theme_change_observer_.Wait();
    // Theme changes propagate asynchronously in DesktopWindowTreeHostX11::
    // FrameTypeChanged(), so ensure all tasks are consumed.
    content::RunAllPendingInMessageLoop();
  }

 private:
  content::WindowedNotificationObserver theme_change_observer_;

  DISALLOW_COPY_AND_ASSIGN(ThemeChangeWaiter);
};

class TestAXEventObserver : public views::AXEventObserver {
 public:
  TestAXEventObserver() { views::AXEventManager::Get()->AddObserver(this); }

  ~TestAXEventObserver() override {
    views::AXEventManager::Get()->RemoveObserver(this);
  }

  // views::AXEventObserver:
  void OnViewEvent(views::View* view, ax::mojom::Event event_type) override {
    if (!view->GetWidget())
      return;
    ui::AXNodeData node_data;
    view->GetAccessibleNodeData(&node_data);
    if (event_type == ax::mojom::Event::kTextChanged &&
        node_data.role == ax::mojom::Role::kListBoxOption)
      text_changed_on_listboxoption_count_++;
    else if (event_type == ax::mojom::Event::kSelectedChildrenChanged)
      selected_children_changed_count_++;
  }

  int text_changed_on_listboxoption_count() {
    return text_changed_on_listboxoption_count_;
  }
  int selected_children_changed_count() {
    return selected_children_changed_count_;
  }

 private:
  int text_changed_on_listboxoption_count_ = 0;
  int selected_children_changed_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestAXEventObserver);
};

}  // namespace

class OmniboxPopupContentsViewTest : public InProcessBrowserTest {
 public:
  OmniboxPopupContentsViewTest() {}

  views::Widget* CreatePopupForTestQuery();
  views::Widget* GetPopupWidget() { return popup_view()->GetWidget(); }
  OmniboxResultView* GetResultViewAt(int index) {
    return popup_view()->result_view_at(index);
  }

  LocationBarView* location_bar() {
    auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->toolbar()->location_bar();
  }
  OmniboxViewViews* omnibox_view() { return location_bar()->omnibox_view(); }
  OmniboxEditModel* edit_model() { return omnibox_view()->model(); }
  OmniboxPopupModel* popup_model() { return edit_model()->popup_model(); }
  OmniboxPopupContentsView* popup_view() {
    return static_cast<OmniboxPopupContentsView*>(popup_model()->view());
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxPopupContentsViewTest);
};

views::Widget* OmniboxPopupContentsViewTest::CreatePopupForTestQuery() {
  EXPECT_TRUE(popup_model()->result().empty());
  EXPECT_FALSE(popup_view()->IsOpen());
  EXPECT_FALSE(GetPopupWidget());

  edit_model()->SetUserText(base::ASCIIToUTF16("foo"));
  AutocompleteInput input(
      base::ASCIIToUTF16("foo"), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  input.set_want_asynchronous_matches(false);
  popup_model()->autocomplete_controller()->Start(input);

  EXPECT_FALSE(popup_model()->result().empty());
  EXPECT_TRUE(popup_view()->IsOpen());
  views::Widget* popup = GetPopupWidget();
  EXPECT_TRUE(popup);
  return popup;
}

// Tests widget alignment of the different popup types.
IN_PROC_BROWSER_TEST_F(OmniboxPopupContentsViewTest, PopupAlignment) {
  views::Widget* popup = CreatePopupForTestQuery();

#if defined(USE_AURA)
  popup_view()->UpdatePopupAppearance();
#endif  // defined(USE_AURA)

  gfx::Rect alignment_rect = location_bar()->GetBoundsInScreen();
  alignment_rect.Inset(
      -RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets());
  alignment_rect.Inset(-RoundedOmniboxResultsFrame::GetShadowInsets());
  // Top, left and right should align. Bottom depends on the results.
  gfx::Rect popup_rect = popup->GetRestoredBounds();
  EXPECT_EQ(popup_rect.y(), alignment_rect.y());
  EXPECT_EQ(popup_rect.x(), alignment_rect.x());
  EXPECT_EQ(popup_rect.right(), alignment_rect.right());
}

// Integration test for omnibox popup theming.
IN_PROC_BROWSER_TEST_F(OmniboxPopupContentsViewTest, ThemeIntegration) {
  // This test relies on the light/dark variants of the result background to be
  // different. But when using the GTK theme on Linux, these colors will be the
  // same. Ensure we're not using the system (GTK) theme, which may be
  // conditionally enabled depending on the environment.
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(browser()->profile());
  if (!theme_service->UsingDefaultTheme()) {
    ThemeChangeWaiter wait(theme_service);
    theme_service->UseDefaultTheme();
  }
  ASSERT_TRUE(theme_service->UsingDefaultTheme());

  Browser* browser_under_test = browser();
  BrowserView::GetBrowserViewForBrowser(browser())
      ->GetNativeTheme()
      ->set_use_dark_colors(false);
  const ui::ThemeProvider* light_theme_provider =
      &ThemeService::GetThemeProviderForProfile(browser_under_test->profile());
  const ui::ThemeProvider* dark_theme_provider =
      &ThemeService::GetThemeProviderForProfile(
          browser_under_test->profile()->GetOffTheRecordProfile());

  // Unthemed, non-incognito always has a white background. Exceptions: Inverted
  // color themes on Windows and GTK (not tested here).
  EXPECT_EQ(SK_ColorWHITE, GetOmniboxColor(light_theme_provider,
                                           OmniboxPart::RESULTS_BACKGROUND));

  // Helper to get the background selected color for |browser_under_test| using
  // omnibox_theme.
  auto get_selection_color = [](const ui::ThemeProvider* theme_provider) {
    return GetOmniboxColor(theme_provider, OmniboxPart::RESULTS_BACKGROUND,
                           OmniboxPartState::SELECTED);
  };

  const SkColor selection_color_light =
      GetOmniboxColor(light_theme_provider, OmniboxPart::RESULTS_BACKGROUND,
                      OmniboxPartState::SELECTED);
  const SkColor selection_color_dark =
      GetOmniboxColor(dark_theme_provider, OmniboxPart::RESULTS_BACKGROUND,
                      OmniboxPartState::SELECTED);

  // Tests below are mainly interested just whether things change, so ensure
  // that can be detected.
  EXPECT_NE(selection_color_dark, selection_color_light);

  EXPECT_EQ(selection_color_light, get_selection_color(light_theme_provider));

  // Check unthemed incognito windows.
  Browser* incognito_browser = CreateIncognitoBrowser();
  browser_under_test = incognito_browser;
  EXPECT_EQ(selection_color_dark, get_selection_color(dark_theme_provider));

  // Install a theme (in both browsers, since it's the same profile).
  extensions::ChromeTestExtensionLoader loader(browser()->profile());
  {
    ThemeChangeWaiter wait(theme_service);
    base::FilePath path = ui_test_utils::GetTestFilePath(
        base::FilePath().AppendASCII("extensions"),
        base::FilePath().AppendASCII("theme"));
    loader.LoadExtension(path);
  }

  // Check the incognito browser first. Everything should now be light.
  EXPECT_EQ(selection_color_light, get_selection_color(light_theme_provider));

  // Same in the non-incognito browser.
  browser_under_test = browser();
  EXPECT_EQ(selection_color_light, get_selection_color(light_theme_provider));

  // Switch to the default theme without installing a custom theme. E.g. this is
  // what gets used on KDE or when switching to the "classic" theme in settings.
  {
    ThemeChangeWaiter wait(theme_service);
    theme_service->UseDefaultTheme();
  }
  EXPECT_EQ(selection_color_light, get_selection_color(light_theme_provider));

  // Check incognito again. It should now use a dark theme, even on Linux.
  browser_under_test = incognito_browser;
  EXPECT_EQ(selection_color_dark, get_selection_color(dark_theme_provider));
}

// TODO(tapted): https://crbug.com/905508 Fix and enable on Mac.
#if defined(OS_MACOSX)
#define MAYBE_ClickOmnibox DISABLED_ClickOmnibox
#else
#define MAYBE_ClickOmnibox ClickOmnibox
#endif
// Test that clicks over the omnibox do not hit the popup.
IN_PROC_BROWSER_TEST_F(OmniboxPopupContentsViewTest, MAYBE_ClickOmnibox) {
  CreatePopupForTestQuery();

  gfx::NativeWindow event_window = browser()->window()->GetNativeWindow();
#if defined(USE_AURA)
  event_window = event_window->GetRootWindow();
#endif
  ui::test::EventGenerator generator(event_window);

  OmniboxResultView* result = GetResultViewAt(0);
  ASSERT_TRUE(result);

  // Sanity check: ensure the EventGenerator clicks where we think it should
  // when clicking on a result (but don't dismiss the popup yet). This will fail
  // if the WindowTreeHost and EventGenerator coordinate systems do not align.
  {
    const gfx::Point expected_point = result->GetLocalBounds().CenterPoint();
    EXPECT_NE(gfx::Point(), expected_point);

    ClickTrackingOverlayView overlay(result);
    generator.MoveMouseTo(result->GetBoundsInScreen().CenterPoint());
    generator.ClickLeftButton();
    auto click = overlay.last_click();
    ASSERT_TRUE(click.has_value());
    ASSERT_EQ(expected_point, click.value());
  }

  // Select the text, so that we can test whether a click is received (which
  // should deselect the text);
  omnibox_view()->SelectAll(true);
  views::Textfield* textfield = omnibox_view();
  EXPECT_EQ(base::ASCIIToUTF16("foo"), textfield->GetSelectedText());

  generator.MoveMouseTo(location_bar()->GetBoundsInScreen().CenterPoint());
  generator.ClickLeftButton();
  EXPECT_EQ(base::string16(), textfield->GetSelectedText());

  // Clicking the result should dismiss the popup (asynchronously).
  generator.MoveMouseTo(result->GetBoundsInScreen().CenterPoint());

  ASSERT_TRUE(GetPopupWidget());
  EXPECT_FALSE(GetPopupWidget()->IsClosed());

  generator.ClickLeftButton();
  ASSERT_TRUE(GetPopupWidget());

  // Instantly finish all queued animations.
  GetPopupWidget()->GetLayer()->GetAnimator()->StopAnimating();
  EXPECT_TRUE(GetPopupWidget()->IsClosed());
}

// Check that the location bar background (and the background of the textfield
// it contains) changes when it receives focus, and matches the popup background
// color.
IN_PROC_BROWSER_TEST_F(OmniboxPopupContentsViewTest,
                       PopupMatchesLocationBarBackground) {
  // In dark mode the omnibox focused and unfocused colors are the same, which
  // makes this test fail; see comments below.
  BrowserView::GetBrowserViewForBrowser(browser())
      ->GetNativeTheme()
      ->set_use_dark_colors(false);

  // Start with the Omnibox unfocused.
  omnibox_view()->GetFocusManager()->ClearFocus();
  const SkColor color_before_focus = location_bar()->background()->get_color();
  EXPECT_EQ(color_before_focus, omnibox_view()->GetBackgroundColor());

  // Give the Omnibox focus and get its focused color.
  omnibox_view()->RequestFocus();
  const SkColor color_after_focus = location_bar()->background()->get_color();

  // Sanity check that the colors are different, otherwise this test will not be
  // testing anything useful. It is possible that a particular theme could
  // configure these colors to be the same. In that case, this test should be
  // updated to detect that, or switch to a theme where they are different.
  EXPECT_NE(color_before_focus, color_after_focus);
  EXPECT_EQ(color_after_focus, omnibox_view()->GetBackgroundColor());

  // The background is hosted in the view that contains the results area.
  CreatePopupForTestQuery();
  views::View* background_host = popup_view()->parent();
  EXPECT_EQ(color_after_focus, background_host->background()->get_color());

  // Blurring the Omnibox should restore the original colors.
  omnibox_view()->GetFocusManager()->ClearFocus();
  EXPECT_EQ(color_before_focus, location_bar()->background()->get_color());
  EXPECT_EQ(color_before_focus, omnibox_view()->GetBackgroundColor());
}

IN_PROC_BROWSER_TEST_F(OmniboxPopupContentsViewTest,
                       EmitTextChangedAccessibilityEvent) {
  // Creation and population of the popup should not result in a text/name
  // change accessibility event.
  TestAXEventObserver observer;
  CreatePopupForTestQuery();
  ACMatches matches;
  AutocompleteMatch match(nullptr, 500, false,
                          AutocompleteMatchType::HISTORY_TITLE);
  AutocompleteController* controller = popup_model()->autocomplete_controller();
  match.contents = base::ASCIIToUTF16("https://foobar.com");
  matches.push_back(match);
  match.contents = base::ASCIIToUTF16("https://foobarbaz.com");
  matches.push_back(match);
  controller->result_.AppendMatches(controller->input_, matches);
  popup_view()->UpdatePopupAppearance();
  EXPECT_EQ(observer.text_changed_on_listboxoption_count(), 0);

  // Changing the user text while in the input rather than the list should not
  // result in a text/name change accessibility event.
  edit_model()->SetUserText(base::ASCIIToUTF16("bar"));
  edit_model()->StartAutocomplete(false, false);
  popup_view()->UpdatePopupAppearance();
  EXPECT_EQ(observer.text_changed_on_listboxoption_count(), 0);

  // Each time the selection changes, we should have a text/name change event.
  // This makes it possible for screen readers to have the updated match content
  // when they are notified the selection changed.
  popup_view()->model()->SetSelectedLine(1, false, false);
  EXPECT_EQ(observer.text_changed_on_listboxoption_count(), 1);

  popup_view()->model()->SetSelectedLine(2, false, false);
  EXPECT_EQ(observer.text_changed_on_listboxoption_count(), 2);
}

IN_PROC_BROWSER_TEST_F(OmniboxPopupContentsViewTest,
                       EmitSelectedChildrenChangedAccessibilityEvent) {
  // Create a popup for the matches.
  GetPopupWidget();
  edit_model()->SetUserText(base::ASCIIToUTF16("foo"));
  AutocompleteInput input(
      base::ASCIIToUTF16("foo"), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  input.set_want_asynchronous_matches(false);
  popup_model()->autocomplete_controller()->Start(input);

  // Create a match to populate the autocomplete.
  base::string16 match_url = base::ASCIIToUTF16("https://foobar.com");
  AutocompleteMatch match(nullptr, 500, false,
                          AutocompleteMatchType::HISTORY_TITLE);
  match.contents = match_url;
  match.contents_class.push_back(
      ACMatchClassification(0, ACMatchClassification::URL));
  match.destination_url = GURL(match_url);
  match.description = base::ASCIIToUTF16("Foobar");
  match.allowed_to_be_default_match = true;

  AutocompleteController* autocomplete_controller =
      popup_model()->autocomplete_controller();
  AutocompleteResult& results = autocomplete_controller->result_;
  ACMatches matches;
  matches.push_back(match);
  results.AppendMatches(input, matches);
  results.SortAndCull(input, nullptr);
  autocomplete_controller->NotifyChanged(true);

  // Lets check that arrowing up and down emits the event.
  TestAXEventObserver observer;
  EXPECT_EQ(observer.selected_children_changed_count(), 0);
  // This is equiverlent of the user arrowing down in the omnibox.
  popup_view()->model()->SetSelectedLine(1, false, false);
  EXPECT_EQ(observer.selected_children_changed_count(), 1);
}
