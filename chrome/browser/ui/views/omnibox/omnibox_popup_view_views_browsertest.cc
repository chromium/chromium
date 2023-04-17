// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/test/theme_service_changed_waiter.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_views.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/theme_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/accessibility/ax_event_observer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/linux/linux_ui.h"
#include "ui/linux/linux_ui_getter.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace {

bool contains(std::string str, std::string substr) {
  return str.find(substr) != std::string::npos;
}

// A View that positions itself over another View to intercept clicks.
class ClickTrackingOverlayView : public views::View {
 public:
  explicit ClickTrackingOverlayView(OmniboxResultView* result) {
    // |result|'s parent is the OmniboxPopupViewViews, which expects that all
    // its children are OmniboxResultViews.  So skip over it and add this to the
    // OmniboxPopupViewViews's parent.
    auto* contents = result->parent();
    SetBoundsRect(contents->ConvertRectToParent(result->bounds()));
    contents->parent()->AddChildView(this);
  }

  // views::View:
  void OnMouseEvent(ui::MouseEvent* event) override {
    last_click_ = event->location();
  }

  absl::optional<gfx::Point> last_click() const { return last_click_; }

 private:
  absl::optional<gfx::Point> last_click_;
};

// Helper to wait for theme changes. The wait is triggered when an instance of
// this class goes out of scope.
class ThemeChangeWaiter {
 public:
  explicit ThemeChangeWaiter(ThemeService* theme_service)
      : waiter_(theme_service) {}

  ThemeChangeWaiter(const ThemeChangeWaiter&) = delete;
  ThemeChangeWaiter& operator=(const ThemeChangeWaiter&) = delete;

  ~ThemeChangeWaiter() {
    waiter_.WaitForThemeChanged();
    // Theme changes propagate asynchronously in DesktopWindowTreeHostX11::
    // FrameTypeChanged(), so ensure all tasks are consumed.
    content::RunAllPendingInMessageLoop();
  }

 private:
  test::ThemeServiceChangedWaiter waiter_;
};

class TestAXEventObserver : public views::AXEventObserver {
 public:
  TestAXEventObserver() { views::AXEventManager::Get()->AddObserver(this); }

  TestAXEventObserver(const TestAXEventObserver&) = delete;
  TestAXEventObserver& operator=(const TestAXEventObserver&) = delete;

  ~TestAXEventObserver() override {
    views::AXEventManager::Get()->RemoveObserver(this);
  }

  // views::AXEventObserver:
  void OnViewEvent(views::View* view, ax::mojom::Event event_type) override {
    if (!view->GetWidget()) {
      return;
    }
    ui::AXNodeData node_data;
    view->GetAccessibleNodeData(&node_data);
    ax::mojom::Role role = node_data.role;
    if (event_type == ax::mojom::Event::kTextChanged &&
        role == ax::mojom::Role::kListBoxOption) {
      text_changed_on_listboxoption_count_++;
    } else if (event_type == ax::mojom::Event::kSelectedChildrenChanged &&
               role == ax::mojom::Role::kListBox) {
      selected_children_changed_count_++;
    } else if (event_type == ax::mojom::Event::kSelection &&
               role == ax::mojom::Role::kListBoxOption) {
      selection_changed_count_++;
      selected_option_name_ =
          node_data.GetStringAttribute(ax::mojom::StringAttribute::kName);
    } else if (event_type == ax::mojom::Event::kValueChanged &&
               role == ax::mojom::Role::kTextField) {
      value_changed_count_++;
      omnibox_value_ =
          node_data.GetStringAttribute(ax::mojom::StringAttribute::kValue);
    } else if (event_type == ax::mojom::Event::kActiveDescendantChanged &&
               role == ax::mojom::Role::kTextField) {
      active_descendant_changed_count_++;
    }
  }

  int text_changed_on_listboxoption_count() {
    return text_changed_on_listboxoption_count_;
  }
  int selected_children_changed_count() {
    return selected_children_changed_count_;
  }
  int selection_changed_count() { return selection_changed_count_; }
  int value_changed_count() { return value_changed_count_; }
  int active_descendant_changed_count() {
    return active_descendant_changed_count_;
  }

  std::string omnibox_value() { return omnibox_value_; }
  std::string selected_option_name() { return selected_option_name_; }

 private:
  int text_changed_on_listboxoption_count_ = 0;
  int selected_children_changed_count_ = 0;
  int selection_changed_count_ = 0;
  int value_changed_count_ = 0;
  int active_descendant_changed_count_ = 0;
  std::string omnibox_value_;
  std::string selected_option_name_;
};

}  // namespace

class OmniboxPopupViewViewsTest : public InProcessBrowserTest {
 public:
  OmniboxPopupViewViewsTest() {}

  OmniboxPopupViewViewsTest(const OmniboxPopupViewViewsTest&) = delete;
  OmniboxPopupViewViewsTest& operator=(const OmniboxPopupViewViewsTest&) =
      delete;

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
  OmniboxPopupViewViews* popup_view() {
    return static_cast<OmniboxPopupViewViews*>(edit_model()->get_popup_view());
  }

  SkColor GetSelectedColor(Browser* browser) {
    return BrowserView::GetBrowserViewForBrowser(browser)
        ->GetColorProvider()
        ->GetColor(kColorOmniboxResultsBackgroundSelected);
  }

  SkColor GetNormalColor(Browser* browser) {
    return BrowserView::GetBrowserViewForBrowser(browser)
        ->GetColorProvider()
        ->GetColor(kColorOmniboxResultsBackground);
  }

  void SetUseDarkColor(bool use_dark) {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    browser_view->GetNativeTheme()->set_use_dark_colors(use_dark);
  }

  void UseDefaultTheme() {
    // Some test relies on the light/dark variants of the result background to
    // be different. But when using the system theme on Linux, these colors will
    // be the same. Ensure we're not using the system theme, which may be
    // conditionally enabled depending on the environment.
#if BUILDFLAG(IS_LINUX)
    // Normally it would be sufficient to call ThemeService::UseDefaultTheme()
    // which sets the kUsesSystemTheme user pref on the browser's profile.
    // However BrowserThemeProvider::GetColorProviderColor() currently does not
    // pass an aura::Window to LinuxUI::GetNativeTheme() - which means that the
    // NativeThemeGtk instance will always be returned.
    // TODO(crbug.com/1304441): Remove this once GTK passthrough is fully
    // supported.
    ui::LinuxUiGetter::set_instance(nullptr);
    ui::NativeTheme::GetInstanceForNativeUi()->NotifyOnNativeThemeUpdated();

    ThemeService* theme_service =
        ThemeServiceFactory::GetForProfile(browser()->profile());
    if (!theme_service->UsingDefaultTheme()) {
      ThemeChangeWaiter wait(theme_service);
      theme_service->UseDefaultTheme();
    }
    ASSERT_TRUE(theme_service->UsingDefaultTheme());
#endif  // BUILDFLAG(IS_LINUX)
  }

  OmniboxTriggeredFeatureService* triggered_feature_service() {
    return &triggered_feature_service_;
  }

 private:
  OmniboxTriggeredFeatureService triggered_feature_service_;
};

views::Widget* OmniboxPopupViewViewsTest::CreatePopupForTestQuery() {
  EXPECT_TRUE(edit_model()->result().empty());
  EXPECT_FALSE(popup_view()->IsOpen());
  EXPECT_FALSE(GetPopupWidget());

  edit_model()->SetUserText(u"foo");
  AutocompleteInput input(
      u"foo", metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  input.set_omit_asynchronous_matches(true);
  edit_model()->autocomplete_controller()->Start(input);

  EXPECT_FALSE(edit_model()->result().empty());
  EXPECT_TRUE(popup_view()->IsOpen());
  views::Widget* popup = GetPopupWidget();
  EXPECT_TRUE(popup);
  return popup;
}

// Tests widget alignment of the different popup types.
IN_PROC_BROWSER_TEST_F(OmniboxPopupViewViewsTest, PopupAlignment) {
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

// Integration test for omnibox popup theming in regular.
IN_PROC_BROWSER_TEST_F(OmniboxPopupViewViewsTest, ThemeIntegration) {
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(browser()->profile());
  UseDefaultTheme();

  SetUseDarkColor(true);
  const SkColor selection_color_dark = GetSelectedColor(browser());

  SetUseDarkColor(false);
  const SkColor selection_color_light = GetSelectedColor(browser());

  // Unthemed, non-incognito always has a white background. Exceptions: Inverted
  // color themes on Windows and GTK (not tested here).
  EXPECT_EQ(SK_ColorWHITE, GetNormalColor(browser()));

  // Tests below are mainly interested just whether things change, so ensure
  // that can be detected.
  EXPECT_NE(selection_color_dark, selection_color_light);

  EXPECT_EQ(selection_color_light, GetSelectedColor(browser()));

  // Install a theme (in both browsers, since it's the same profile).
  extensions::ChromeTestExtensionLoader loader(browser()->profile());
  {
    ThemeChangeWaiter wait(theme_service);
    base::FilePath path = ui_test_utils::GetTestFilePath(
        base::FilePath().AppendASCII("extensions"),
        base::FilePath().AppendASCII("theme"));
    loader.LoadExtension(path);
  }

  // Same in the non-incognito browser.
  EXPECT_EQ(selection_color_light, GetSelectedColor(browser()));

  // Switch to the default theme without installing a custom theme. E.g. this is
  // what gets used on KDE or when switching to the "classic" theme in settings.
  UseDefaultTheme();

  EXPECT_EQ(selection_color_light, GetSelectedColor(browser()));
}

// Integration test for omnibox popup theming in Incognito.
IN_PROC_BROWSER_TEST_F(OmniboxPopupViewViewsTest, ThemeIntegrationInIncognito) {
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(browser()->profile());
  UseDefaultTheme();

  SetUseDarkColor(true);
  const SkColor selection_color_dark = GetSelectedColor(browser());

  SetUseDarkColor(false);

  // Install a theme (in both browsers, since it's the same profile).
  extensions::ChromeTestExtensionLoader loader(browser()->profile());
  {
    ThemeChangeWaiter wait(theme_service);
    base::FilePath path = ui_test_utils::GetTestFilePath(
        base::FilePath().AppendASCII("extensions"),
        base::FilePath().AppendASCII("theme"));
    loader.LoadExtension(path);
  }

  // Check unthemed incognito windows.
  Browser* incognito_browser = CreateIncognitoBrowser();

  EXPECT_EQ(selection_color_dark, GetSelectedColor(incognito_browser));
  // Switch to the default theme without installing a custom theme. E.g. this is
  // what gets used on KDE or when switching to the "classic" theme in settings.
  UseDefaultTheme();

  // Check incognito again. It should continue to use a dark theme, even on
  // Linux.
  EXPECT_EQ(selection_color_dark, GetSelectedColor(incognito_browser));
}

// TODO(tapted): https://crbug.com/905508 Fix and enable on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ClickOmnibox DISABLED_ClickOmnibox
#else
#define MAYBE_ClickOmnibox ClickOmnibox
#endif
// Test that clicks over the omnibox do not hit the popup.
IN_PROC_BROWSER_TEST_F(OmniboxPopupViewViewsTest, MAYBE_ClickOmnibox) {
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
  EXPECT_EQ(u"foo", textfield->GetSelectedText());

  generator.MoveMouseTo(location_bar()->GetBoundsInScreen().CenterPoint());
  generator.ClickLeftButton();
  EXPECT_EQ(std::u16string(), textfield->GetSelectedText());

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
// Flaky on Linux and Windows. See https://crbug.com/1120701
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#define MAYBE_PopupMatchesLocationBarBackground \
  DISABLED_PopupMatchesLocationBarBackground
#else
#define MAYBE_PopupMatchesLocationBarBackground \
  PopupMatchesLocationBarBackground
#endif
IN_PROC_BROWSER_TEST_F(OmniboxPopupViewViewsTest,
                       MAYBE_PopupMatchesLocationBarBackground) {
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

// Flaky on Mac: https://crbug.com/1140153.
#if BUILDFLAG(IS_MAC)
#define MAYBE_EmitAccessibilityEvents DISABLED_EmitAccessibilityEvents
#else
#define MAYBE_EmitAccessibilityEvents EmitAccessibilityEvents
#endif
IN_PROC_BROWSER_TEST_F(OmniboxPopupViewViewsTest,
                       MAYBE_EmitAccessibilityEvents) {
  // Creation and population of the popup should not result in a text/name
  // change accessibility event.
  TestAXEventObserver observer;
  CreatePopupForTestQuery();
  ACMatches matches;
  AutocompleteMatch match(nullptr, 500, false,
                          AutocompleteMatchType::HISTORY_TITLE);
  AutocompleteController* controller = edit_model()->autocomplete_controller();
  match.contents = u"https://foobar.com";
  match.description = u"FooBarCom";
  match.contents_class = {{0, 0}};
  match.description_class = {{0, 0}};
  matches.push_back(match);
  match.contents = u"https://foobarbaz.com";
  match.description = u"FooBarBazCom";
  match.contents_class = {{0, 0}};
  match.description_class = {{0, 0}};
  matches.push_back(match);
  controller->result_.AppendMatches(matches);
  popup_view()->UpdatePopupAppearance();
  EXPECT_EQ(observer.text_changed_on_listboxoption_count(), 0);

  // Changing the user text while in the input rather than the list should not
  // result in a text/name change accessibility event.
  edit_model()->SetUserText(u"bar");
  edit_model()->StartAutocomplete(false, false);
  popup_view()->UpdatePopupAppearance();
  EXPECT_EQ(observer.text_changed_on_listboxoption_count(), 0);
  EXPECT_EQ(observer.selected_children_changed_count(), 1);
  EXPECT_EQ(observer.selection_changed_count(), 1);
  EXPECT_EQ(observer.active_descendant_changed_count(), 1);
  EXPECT_EQ(observer.value_changed_count(), 2);

  // Each time the selection changes, we should have a text/name change event.
  // This makes it possible for screen readers to have the updated match content
  // when they are notified the selection changed.
  edit_model()->SetPopupSelection(OmniboxPopupSelection(1));
  EXPECT_EQ(observer.text_changed_on_listboxoption_count(), 1);
  EXPECT_EQ(observer.selected_children_changed_count(), 2);
  EXPECT_EQ(observer.selection_changed_count(), 2);
  EXPECT_EQ(observer.active_descendant_changed_count(), 2);
  EXPECT_EQ(observer.value_changed_count(), 3);
  EXPECT_TRUE(contains(observer.omnibox_value(), "2 of 3"));
  EXPECT_FALSE(contains(observer.selected_option_name(), "2 of 3"));
  EXPECT_TRUE(contains(observer.selected_option_name(), "foobar.com"));
  EXPECT_TRUE(contains(observer.omnibox_value(), "FooBarCom"));
  EXPECT_TRUE(contains(observer.selected_option_name(), "FooBarCom"));
  EXPECT_TRUE(contains(observer.omnibox_value(), "location from history"));
  EXPECT_TRUE(
      contains(observer.selected_option_name(), "location from history"));

  edit_model()->SetPopupSelection(OmniboxPopupSelection(2));
  EXPECT_EQ(observer.text_changed_on_listboxoption_count(), 2);
  EXPECT_EQ(observer.selected_children_changed_count(), 3);
  EXPECT_EQ(observer.selection_changed_count(), 3);
  EXPECT_EQ(observer.active_descendant_changed_count(), 3);
  EXPECT_EQ(observer.value_changed_count(), 4);
  EXPECT_TRUE(contains(observer.omnibox_value(), "3 of 3"));
  EXPECT_FALSE(contains(observer.selected_option_name(), "3 of 3"));
  EXPECT_TRUE(contains(observer.selected_option_name(), "foobarbaz.com"));
  EXPECT_TRUE(contains(observer.omnibox_value(), "FooBarBazCom"));
  EXPECT_TRUE(contains(observer.selected_option_name(), "FooBarBazCom"));

  // Check that active descendant on textbox matches the selected result view.
  ui::AXNodeData ax_node_data_omnibox;
  omnibox_view()->GetAccessibleNodeData(&ax_node_data_omnibox);
  OmniboxResultView* selected_result_view = GetResultViewAt(2);
  EXPECT_EQ(ax_node_data_omnibox.GetIntAttribute(
                ax::mojom::IntAttribute::kActivedescendantId),
            selected_result_view->GetViewAccessibility().GetUniqueId().Get());
}

// Flaky on Mac: https://crbug.com/1146627.
#if BUILDFLAG(IS_MAC)
#define MAYBE_EmitAccessibilityEventsOnButtonFocusHint \
  DISABLED_EmitAccessibilityEventsOnButtonFocusHint
#else
#define MAYBE_EmitAccessibilityEventsOnButtonFocusHint \
  EmitAccessibilityEventsOnButtonFocusHint
#endif
IN_PROC_BROWSER_TEST_F(OmniboxPopupViewViewsTest,
                       MAYBE_EmitAccessibilityEventsOnButtonFocusHint) {
  TestAXEventObserver observer;
  CreatePopupForTestQuery();
  ACMatches matches;
  AutocompleteMatch match(nullptr, 500, false,
                          AutocompleteMatchType::HISTORY_TITLE);
  AutocompleteController* controller = edit_model()->autocomplete_controller();
  match.contents = u"https://foobar.com";
  match.description = u"The Foo Of All Bars";
  match.has_tab_match = true;
  matches.push_back(match);
  controller->result_.AppendMatches(matches);
  controller->NotifyChanged();
  popup_view()->UpdatePopupAppearance();

  edit_model()->SetPopupSelection(OmniboxPopupSelection(1));
  EXPECT_EQ(observer.selected_children_changed_count(), 2);
  EXPECT_EQ(observer.selection_changed_count(), 2);
  EXPECT_EQ(observer.active_descendant_changed_count(), 2);
  EXPECT_EQ(observer.value_changed_count(), 2);
  EXPECT_TRUE(contains(observer.omnibox_value(), "The Foo Of All Bars"));
  EXPECT_TRUE(contains(observer.selected_option_name(), "foobar.com"));
  EXPECT_TRUE(contains(observer.omnibox_value(), "press Tab then Enter"));
  EXPECT_TRUE(contains(observer.omnibox_value(), "2 of 2"));
  EXPECT_TRUE(
      contains(observer.selected_option_name(), "press Tab then Enter"));
  EXPECT_FALSE(contains(observer.selected_option_name(), "2 of 2"));

  edit_model()->SetPopupSelection(OmniboxPopupSelection(
      1, OmniboxPopupSelection::FOCUSED_BUTTON_TAB_SWITCH));
  EXPECT_TRUE(contains(observer.omnibox_value(), "The Foo Of All Bars"));
  EXPECT_EQ(observer.selected_children_changed_count(), 3);
  EXPECT_EQ(observer.selection_changed_count(), 3);
  EXPECT_EQ(observer.active_descendant_changed_count(), 3);
  EXPECT_EQ(observer.value_changed_count(), 3);
  EXPECT_TRUE(contains(observer.omnibox_value(), "press Enter to switch"));
  EXPECT_FALSE(contains(observer.omnibox_value(), "2 of 2"));
  EXPECT_TRUE(
      contains(observer.selected_option_name(), "press Enter to switch"));
  EXPECT_FALSE(contains(observer.selected_option_name(), "2 of 2"));

  edit_model()->SetPopupSelection(
      OmniboxPopupSelection(1, OmniboxPopupSelection::NORMAL));
  EXPECT_TRUE(contains(observer.omnibox_value(), "The Foo Of All Bars"));
  EXPECT_TRUE(contains(observer.selected_option_name(), "foobar.com"));
  EXPECT_EQ(observer.selected_children_changed_count(), 4);
  EXPECT_EQ(observer.selection_changed_count(), 4);
  EXPECT_EQ(observer.active_descendant_changed_count(), 4);
  EXPECT_EQ(observer.value_changed_count(), 4);
  EXPECT_TRUE(contains(observer.omnibox_value(), "press Tab then Enter"));
  EXPECT_TRUE(contains(observer.omnibox_value(), "2 of 2"));
  EXPECT_TRUE(
      contains(observer.selected_option_name(), "press Tab then Enter"));
  EXPECT_FALSE(contains(observer.selected_option_name(), "2 of 2"));
}

IN_PROC_BROWSER_TEST_F(OmniboxPopupViewViewsTest,
                       EmitSelectedChildrenChangedAccessibilityEvent) {
  // Create a popup for the matches.
  GetPopupWidget();
  edit_model()->SetUserText(u"foo");
  AutocompleteInput input(
      u"foo", metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  input.set_omit_asynchronous_matches(true);
  edit_model()->autocomplete_controller()->Start(input);

  // Create a match to populate the autocomplete.
  std::u16string match_url = u"https://foobar.com";
  AutocompleteMatch match(nullptr, 500, false,
                          AutocompleteMatchType::HISTORY_TITLE);
  match.contents = match_url;
  match.contents_class.push_back(
      ACMatchClassification(0, ACMatchClassification::URL));
  match.destination_url = GURL(match_url);
  match.description = u"Foobar";
  match.allowed_to_be_default_match = true;

  AutocompleteController* autocomplete_controller =
      edit_model()->autocomplete_controller();
  AutocompleteResult& results = autocomplete_controller->result_;
  ACMatches matches;
  matches.push_back(match);
  results.AppendMatches(matches);
  results.SortAndCull(input, /*template_url_service=*/nullptr,
                      triggered_feature_service());
  autocomplete_controller->NotifyChanged();

  // Check that arrowing up and down emits the event.
  TestAXEventObserver observer;
  EXPECT_EQ(observer.selected_children_changed_count(), 0);
  EXPECT_EQ(observer.selection_changed_count(), 0);
  EXPECT_EQ(observer.value_changed_count(), 0);
  EXPECT_EQ(observer.active_descendant_changed_count(), 0);

  // This is equivalent of the user arrowing down in the omnibox.
  edit_model()->SetPopupSelection(OmniboxPopupSelection(1));
  EXPECT_EQ(observer.selected_children_changed_count(), 1);
  EXPECT_EQ(observer.selection_changed_count(), 1);
  EXPECT_EQ(observer.value_changed_count(), 1);
  EXPECT_EQ(observer.active_descendant_changed_count(), 1);

  // This is equivalent of the user arrowing up in the omnibox.
  edit_model()->SetPopupSelection(OmniboxPopupSelection(0));
  EXPECT_EQ(observer.selected_children_changed_count(), 2);
  EXPECT_EQ(observer.selection_changed_count(), 2);
  EXPECT_EQ(observer.value_changed_count(), 2);
  EXPECT_EQ(observer.active_descendant_changed_count(), 2);

  // TODO(accessibility) Test that closing the popup fires an activedescendant
  //  changed event.

  // Check accessibility of list box while it's open.
  ui::AXNodeData popup_node_data_while_open;
  popup_view()->GetAccessibleNodeData(&popup_node_data_while_open);
  EXPECT_EQ(popup_node_data_while_open.role, ax::mojom::Role::kListBox);
  EXPECT_TRUE(popup_node_data_while_open.HasState(ax::mojom::State::kExpanded));
  EXPECT_FALSE(
      popup_node_data_while_open.HasState(ax::mojom::State::kCollapsed));
  EXPECT_FALSE(
      popup_node_data_while_open.HasState(ax::mojom::State::kInvisible));
  EXPECT_TRUE(popup_node_data_while_open.HasIntAttribute(
      ax::mojom::IntAttribute::kPopupForId));
}
