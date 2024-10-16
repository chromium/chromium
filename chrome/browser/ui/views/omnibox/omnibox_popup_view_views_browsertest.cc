// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_views.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/test/theme_service_changed_waiter.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_views_test.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/actions/tab_switch_action.h"
#include "components/omnibox/browser/fake_autocomplete_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_provider_utils.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/accessibility/ax_event_observer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace {

bool contains(std::string str, std::string substr) {
  return str.find(substr) != std::string::npos;
}

// A View that positions itself over another View to intercept clicks.
class ClickTrackingOverlayView : public views::View {
  METADATA_HEADER(ClickTrackingOverlayView, views::View)

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

  std::optional<gfx::Point> last_click() const { return last_click_; }

 private:
  std::optional<gfx::Point> last_click_;
};

BEGIN_METADATA(ClickTrackingOverlayView)
END_METADATA

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
    view->GetViewAccessibility().GetAccessibleNodeData(&node_data);
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
  SetUseDeviceTheme(false);

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
  // TODO(khalidpeer): Delete this clause once CR23 colors are supported on
  //   themed clients. Currently themed clients fall back to pre-CR23 colors.
  EXPECT_NE(selection_color_light, GetSelectedColor(browser()));

  // Switch to the default theme without installing a custom theme. E.g. this is
  // what gets used on KDE or when switching to the "classic" theme in settings.
  UseDefaultTheme();

  // Given that `UseDefaultTheme()` only changes the theme on Linux (i.e. the
  // call is a no-op on non-Linux platforms), the following test logic is
  // limited to executing only on the Linux platform.
#if BUILDFLAG(IS_LINUX)
  EXPECT_EQ(selection_color_light, GetSelectedColor(browser()));
#endif  // BUILDFLAG(IS_LINUX)
}

IN_PROC_BROWSER_TEST_F(OmniboxPopupViewViewsTest, ThemeIntegrationInIncognito) {
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(browser()->profile());
  UseDefaultTheme();
  SetUseDeviceTheme(false);

  SetUseDarkColor(true);
  SetIsGrayscale(true);

  const SkColor selection_color_dark = GetSelectedColor(browser());

  SetUseDarkColor(false);
  SetIsGrayscale(false);

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
  // TODO(https://crbug.com/329235190): Should be an interactive_ui_test if
  // omnibox popup is an accelerated widget.
  if (views::test::IsOzoneBubblesUsingPlatformWidgets()) {
    GTEST_SKIP();
  }

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

  // The omnibox likes to select all when it becomes focused which can happen
  // when we send a click. To avoid this, send a drag that won't trigger a
  // click.
  gfx::Point click_point = location_bar()->GetBoundsInScreen().CenterPoint();
  gfx::Point release_point(click_point.x() + 50, click_point.y());
  // Sanity check that our drag doesn't count as a click.
  ASSERT_TRUE(
      omnibox_view()->ExceededDragThreshold(release_point - click_point));

  generator.MoveMouseTo(click_point);
  generator.PressLeftButton();
  generator.MoveMouseTo(release_point);
  generator.ReleaseLeftButton();
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
  base::HistogramTester histogram_tester;
  TestAXEventObserver observer;
  CreatePopupForTestQuery();
  ACMatches matches;
  AutocompleteMatch match(nullptr, 500, false,
                          AutocompleteMatchType::HISTORY_TITLE);
  match.destination_url = GURL("https://foobar.com");
  match.contents = u"https://foobar.com";
  match.description = u"FooBarCom";
  match.contents_class = {{0, 0}};
  match.description_class = {{0, 0}};
  matches.push_back(match);
  match.destination_url = GURL("https://foobarbaz.com");
  match.contents = u"https://foobarbaz.com";
  match.description = u"FooBarBazCom";
  match.contents_class = {{0, 0}};
  match.description_class = {{0, 0}};
  matches.push_back(match);
  controller()->autocomplete_controller()->internal_result_.AppendMatches(
      matches);
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
  omnibox_view()->GetViewAccessibility().GetAccessibleNodeData(
      &ax_node_data_omnibox);
  OmniboxResultView* selected_result_view = GetResultViewAt(2);
  EXPECT_EQ(ax_node_data_omnibox.GetIntAttribute(
                ax::mojom::IntAttribute::kActivedescendantId),
            selected_result_view->GetViewAccessibility().GetUniqueId());

  ui::AXNodeData result_node_data;
  selected_result_view->GetViewAccessibility().GetAccessibleNodeData(
      &result_node_data);
  int result_size = static_cast<int>(
      controller()->autocomplete_controller()->result().size());
  EXPECT_EQ(result_size, result_node_data.GetIntAttribute(
                             ax::mojom::IntAttribute::kSetSize));
  histogram_tester.ExpectUniqueSample("Omnibox.Views.PopupFirstPaint", 1, 0);
}

IN_PROC_BROWSER_TEST_F(OmniboxPopupViewViewsTest,
                       AccessibleSelectionOnResultSelection) {
  CreatePopupForTestQuery();
  ACMatches matches;
  AutocompleteMatch match(nullptr, 500, false,
                          AutocompleteMatchType::HISTORY_TITLE);
  match.destination_url = GURL("https://foobar.com");
  match.contents = u"https://foobar.com";
  match.description = u"FooBarCom";
  match.contents_class = {{0, 0}};
  match.description_class = {{0, 0}};
  matches.push_back(match);
  match.destination_url = GURL("https://foobarbaz.com");
  match.contents = u"https://foobarbaz.com";
  match.description = u"FooBarBazCom";
  match.contents_class = {{0, 0}};
  match.description_class = {{0, 0}};
  matches.push_back(match);
  controller()->autocomplete_controller()->internal_result_.AppendMatches(
      matches);
  popup_view()->UpdatePopupAppearance();
  edit_model()->SetUserText(u"bar");
  edit_model()->StartAutocomplete(false, false);
  popup_view()->UpdatePopupAppearance();

  edit_model()->SetPopupSelection(OmniboxPopupSelection(1));
  OmniboxResultView* selected_result_view = GetResultViewAt(1);
  ui::AXNodeData node_data_omnibox_result_view;
  selected_result_view->GetViewAccessibility().GetAccessibleNodeData(
      &node_data_omnibox_result_view);
  EXPECT_TRUE(node_data_omnibox_result_view.GetBoolAttribute(
      ax::mojom::BoolAttribute::kSelected));

  edit_model()->SetPopupSelection(OmniboxPopupSelection(2));
  OmniboxResultView* unselected_result_view = GetResultViewAt(1);
  node_data_omnibox_result_view = ui::AXNodeData();
  unselected_result_view->GetViewAccessibility().GetAccessibleNodeData(
      &node_data_omnibox_result_view);
  EXPECT_FALSE(node_data_omnibox_result_view.GetBoolAttribute(
      ax::mojom::BoolAttribute::kSelected));
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
  base::HistogramTester histogram_tester;
  TestAXEventObserver observer;
  CreatePopupForTestQuery();
  ACMatches matches;
  AutocompleteMatch match(nullptr, 500, false,
                          AutocompleteMatchType::HISTORY_TITLE);
  match.contents = u"https://foobar.com";
  match.description = u"The Foo Of All Bars";
  match.has_tab_match = true;
  match.actions.push_back(base::MakeRefCounted<TabSwitchAction>(GURL()));
  matches.push_back(match);
  controller()->autocomplete_controller()->internal_result_.AppendMatches(
      matches);
  controller()->autocomplete_controller()->NotifyChanged();
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

  edit_model()->SetPopupSelection(
      OmniboxPopupSelection(1, OmniboxPopupSelection::FOCUSED_BUTTON_ACTION));
  EXPECT_TRUE(contains(observer.omnibox_value(), "Tab switch button"));
  EXPECT_EQ(observer.selected_children_changed_count(), 3);
  EXPECT_EQ(observer.selection_changed_count(), 3);
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
  EXPECT_EQ(observer.value_changed_count(), 4);
  EXPECT_TRUE(contains(observer.omnibox_value(), "press Tab then Enter"));
  EXPECT_TRUE(contains(observer.omnibox_value(), "2 of 2"));
  EXPECT_TRUE(
      contains(observer.selected_option_name(), "press Tab then Enter"));
  EXPECT_FALSE(contains(observer.selected_option_name(), "2 of 2"));
  histogram_tester.ExpectUniqueSample("Omnibox.Views.PopupFirstPaint", 1, 0);
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
  controller()->autocomplete_controller()->Start(input);

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

  AutocompleteResult& results =
      controller()->autocomplete_controller()->internal_result_;
  ACMatches matches;
  matches.push_back(match);
  results.AppendMatches(matches);
  results.SortAndCull(input, /*template_url_service=*/nullptr,
                      triggered_feature_service());
  controller()->autocomplete_controller()->NotifyChanged();

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
  popup_view()->GetViewAccessibility().GetAccessibleNodeData(
      &popup_node_data_while_open);
  EXPECT_EQ(popup_node_data_while_open.role, ax::mojom::Role::kListBox);
  EXPECT_TRUE(popup_node_data_while_open.HasState(ax::mojom::State::kExpanded));
  EXPECT_FALSE(
      popup_node_data_while_open.HasState(ax::mojom::State::kCollapsed));
  EXPECT_FALSE(
      popup_node_data_while_open.HasState(ax::mojom::State::kInvisible));
  EXPECT_TRUE(popup_node_data_while_open.HasIntAttribute(
      ax::mojom::IntAttribute::kPopupForId));

  // Check accessibility of list box while it's closed.
  controller()->autocomplete_controller()->Stop(true);
  popup_view()->UpdatePopupAppearance();
  popup_node_data_while_open = ui::AXNodeData();
  popup_view()->GetViewAccessibility().GetAccessibleNodeData(
      &popup_node_data_while_open);
  EXPECT_FALSE(
      popup_node_data_while_open.HasState(ax::mojom::State::kExpanded));
  EXPECT_TRUE(
      popup_node_data_while_open.HasState(ax::mojom::State::kCollapsed));
  EXPECT_TRUE(
      popup_node_data_while_open.HasState(ax::mojom::State::kInvisible));
}

IN_PROC_BROWSER_TEST_F(OmniboxPopupViewViewsTest,
                       AccessibilityStatesOnWidgetDestroyed) {
  // Create a popup for the matches.
  views::Widget* widget = CreatePopupForTestQuery();
  views::test::WidgetDestroyedWaiter waiter(widget);

  // Check accessibility of list box while it's open.
  ui::AXNodeData popup_node_data;
  popup_view()->GetViewAccessibility().GetAccessibleNodeData(&popup_node_data);
  EXPECT_EQ(popup_node_data.role, ax::mojom::Role::kListBox);
  EXPECT_TRUE(popup_node_data.HasState(ax::mojom::State::kExpanded));
  EXPECT_FALSE(popup_node_data.HasState(ax::mojom::State::kCollapsed));
  EXPECT_FALSE(popup_node_data.HasState(ax::mojom::State::kInvisible));
  EXPECT_TRUE(
      popup_node_data.HasIntAttribute(ax::mojom::IntAttribute::kPopupForId));

  // Check accessibility of list box while it's closed.
  widget->Close();
  waiter.Wait();
  EXPECT_FALSE(popup_view()->IsOpen());
  popup_node_data = ui::AXNodeData();
  popup_view()->GetViewAccessibility().GetAccessibleNodeData(&popup_node_data);
  EXPECT_FALSE(popup_node_data.HasState(ax::mojom::State::kExpanded));
  EXPECT_TRUE(popup_node_data.HasState(ax::mojom::State::kCollapsed));
  EXPECT_TRUE(popup_node_data.HasState(ax::mojom::State::kInvisible));
}

IN_PROC_BROWSER_TEST_F(OmniboxPopupViewViewsTest, DeleteSuggestion) {
  scoped_refptr<FakeAutocompleteProvider> provider =
      new FakeAutocompleteProvider(AutocompleteProvider::TYPE_SEARCH);
  controller()->autocomplete_controller()->providers_.push_back(provider);

  ACMatches matches;
  {
    std::u16string match_url = u"https://example.com/";
    AutocompleteMatch match(nullptr, 500, /*deletable=*/true,
                            AutocompleteMatchType::HISTORY_TITLE);
    match.contents = match_url;
    match.contents_class.emplace_back(0, ACMatchClassification::URL);
    match.destination_url = GURL(match_url);
    match.description = u"Deletable Match";
    match.description_class.emplace_back(0, ACMatchClassification::URL);
    match.allowed_to_be_default_match = true;
    match.provider = provider.get();
    matches.push_back(match);
  }
  {
    std::u16string match_url = u"https://google.com/";
    AutocompleteMatch match(nullptr, 500, false,
                            AutocompleteMatchType::HISTORY_TITLE);
    match.contents = match_url;
    match.contents_class.emplace_back(0, ACMatchClassification::URL);
    match.destination_url = GURL(match_url);
    match.description = u"Other Match";
    match.description_class.emplace_back(0, ACMatchClassification::URL);
    match.allowed_to_be_default_match = true;
    match.provider = provider.get();
    metrics::OmniboxScoringSignals scoring_signals;
    scoring_signals.set_first_bookmark_title_match_position(3);
    scoring_signals.set_allowed_to_be_default_match(true);
    scoring_signals.set_length_of_url(20);
    match.scoring_signals = scoring_signals;
    matches.push_back(match);
  }
  provider->matches_ = matches;

  edit_model()->SetUserText(u"foo");
  AutocompleteInput input(
      u"foo", metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  input.set_omit_asynchronous_matches(true);
  controller()->autocomplete_controller()->Start(input);
  ASSERT_TRUE(popup_view()->IsOpen());

  // Select deletable match at index 1 (input is index 0).
  edit_model()->SetPopupSelection(OmniboxPopupSelection(1));
  OmniboxResultView* result_view = popup_view()->result_view_at(1);
  EXPECT_EQ(u"Deletable Match", result_view->match_.contents);
  EXPECT_TRUE(result_view->remove_suggestion_button_->GetVisible());

  // Lay out `remove_suggestion_button_` so that it has non-zero size.
  views::test::RunScheduledLayout(result_view);

  // Click button.
  gfx::Rect button_local_bounds =
      result_view->remove_suggestion_button_->GetLocalBounds();
  ui::MouseEvent mouse_pressed_event(
      ui::EventType::kMousePressed, button_local_bounds.CenterPoint(),
      button_local_bounds.CenterPoint(), base::TimeTicks(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  result_view->remove_suggestion_button_->OnMousePressed(mouse_pressed_event);
  ui::MouseEvent mouse_released_event(
      ui::EventType::kMouseReleased, button_local_bounds.CenterPoint(),
      button_local_bounds.CenterPoint(), base::TimeTicks(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  result_view->remove_suggestion_button_->OnMouseReleased(mouse_released_event);

  EXPECT_EQ(1u, provider->deleted_matches_.size());
  // Make sure the deleted match's OmniboxResultView was hidden.
  // (OmniboxResultViews are never deleted.)
  int visible_children = 0;
  for (views::View* child : popup_view()->children()) {
    if (child->GetVisible()) {
      visible_children++;
    }
  }
  EXPECT_EQ(2, visible_children);
  EXPECT_EQ(u"foo", popup_view()->result_view_at(0)->match_.contents);
  EXPECT_EQ(u"Other Match", popup_view()->result_view_at(1)->match_.contents);
  EXPECT_EQ(OmniboxPopupSelection(1), edit_model()->GetPopupSelection());
}

// Flaky on Mac: https://crbug.com/1511356
// Flaky on Win: https://crbug.com/365250293
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_SpaceEntersKeywordMode DISABLED_SpaceEntersKeywordMode
#else
#define MAYBE_SpaceEntersKeywordMode SpaceEntersKeywordMode
#endif
IN_PROC_BROWSER_TEST_F(OmniboxPopupViewViewsTest,
                       MAYBE_SpaceEntersKeywordMode) {
  CreatePopupForTestQuery();
  EXPECT_TRUE(popup_view()->IsOpen());

  omnibox_view()->controller()->client()->GetPrefs()->SetBoolean(
      omnibox::kKeywordSpaceTriggeringEnabled, true);
  omnibox_view()->SetUserText(u"@bookmarks");
  edit_model()->StartAutocomplete(false, false);
  popup_view()->UpdatePopupAppearance();

  EXPECT_FALSE(edit_model()->is_keyword_selected());
  ui::KeyEvent space(ui::EventType::kKeyPressed, ui::VKEY_SPACE, 0);
  omnibox_view()->OnKeyEvent(&space);
  EXPECT_TRUE(edit_model()->is_keyword_selected());
}

IN_PROC_BROWSER_TEST_F(OmniboxPopupViewViewsTest,
                       AccesibilityAttributePopupForId) {
  CreatePopupForTestQuery();
  popup_view()->UpdatePopupAppearance();
  edit_model()->SetPopupSelection(OmniboxPopupSelection(0));

  ui::AXNodeData ax_node_data_omnibox;
  popup_view()->GetViewAccessibility().GetAccessibleNodeData(
      &ax_node_data_omnibox);
  EXPECT_TRUE(ax_node_data_omnibox.HasIntAttribute(
      ax::mojom::IntAttribute::kPopupForId));
  EXPECT_EQ(ax_node_data_omnibox.GetIntAttribute(
                ax::mojom::IntAttribute::kPopupForId),
            omnibox_view()->GetViewAccessibility().GetUniqueId());
}

// Changing selection in omnibox popup view should update activedescendant id in
// omnibox view.
IN_PROC_BROWSER_TEST_F(OmniboxPopupViewViewsTest,
                       AccessibleActivedescendantId) {
  ui::AXNodeData ax_node_data_omnibox;
  omnibox_view()->GetViewAccessibility().GetAccessibleNodeData(
      &ax_node_data_omnibox);
  EXPECT_FALSE(popup_view()->IsOpen());
  EXPECT_FALSE(ax_node_data_omnibox.HasIntAttribute(
      ax::mojom::IntAttribute::kActivedescendantId));

  CreatePopupForTestQuery();
  ACMatches matches;
  AutocompleteMatch match(nullptr, 500, false,
                          AutocompleteMatchType::HISTORY_TITLE);
  match.destination_url = GURL("https://foobar.com");
  match.contents = u"https://foobar.com";
  match.description = u"FooBarCom";
  match.contents_class = {{0, 0}};
  match.description_class = {{0, 0}};
  matches.push_back(match);
  match.destination_url = GURL("https://foobarbaz.com");
  match.contents = u"https://foobarbaz.com";
  match.description = u"FooBarBazCom";
  match.contents_class = {{0, 0}};
  match.description_class = {{0, 0}};
  matches.push_back(match);
  controller()->autocomplete_controller()->internal_result_.AppendMatches(
      matches);

  // Check accessibility when popup is open.
  ax_node_data_omnibox = ui::AXNodeData();
  edit_model()->StartAutocomplete(false, false);
  omnibox_view()->GetViewAccessibility().GetAccessibleNodeData(
      &ax_node_data_omnibox);
  EXPECT_TRUE(popup_view()->IsOpen());
  EXPECT_TRUE(ax_node_data_omnibox.HasIntAttribute(
      ax::mojom::IntAttribute::kActivedescendantId));
  // First result is selected by default.
  EXPECT_EQ(ax_node_data_omnibox.GetIntAttribute(
                ax::mojom::IntAttribute::kActivedescendantId),
            GetResultViewAt(0)->GetViewAccessibility().GetUniqueId());

  ax_node_data_omnibox = ui::AXNodeData();
  edit_model()->SetPopupSelection(OmniboxPopupSelection(1));
  omnibox_view()->GetViewAccessibility().GetAccessibleNodeData(
      &ax_node_data_omnibox);
  EXPECT_EQ(ax_node_data_omnibox.GetIntAttribute(
                ax::mojom::IntAttribute::kActivedescendantId),
            GetResultViewAt(1)->GetViewAccessibility().GetUniqueId());

  // Check accessibility when popup is closed.
  ax_node_data_omnibox = ui::AXNodeData();
  controller()->autocomplete_controller()->Stop(true);
  omnibox_view()->GetViewAccessibility().GetAccessibleNodeData(
      &ax_node_data_omnibox);
  EXPECT_FALSE(popup_view()->IsOpen());
  EXPECT_FALSE(ax_node_data_omnibox.HasIntAttribute(
      ax::mojom::IntAttribute::kActivedescendantId));
}
