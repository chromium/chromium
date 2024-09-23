// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/adapters.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/command_updater_impl.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_client.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/lookalikes/core/safety_tip_test_utils.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/test_location_bar_model.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_edit_commands.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/animation/animation_container_element.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/render_text_test_api.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/textfield/textfield_test_api.h"

using gfx::Range;
using metrics::OmniboxEventProto;

class TestingOmniboxView;

// TestingOmniboxView ---------------------------------------------------------

class TestingOmniboxView : public OmniboxViewViews {
 public:
  explicit TestingOmniboxView(std::unique_ptr<OmniboxClient> client,
                              bool popup_window_mode);
  TestingOmniboxView(const TestingOmniboxView&) = delete;
  TestingOmniboxView& operator=(const TestingOmniboxView&) = delete;

  using views::Textfield::GetRenderText;

  void ResetEmphasisTestState();

  void CheckUpdatePopupCallInfo(size_t call_count,
                                std::u16string_view text,
                                const Range& selection_range);

  void CheckUpdatePopupNotCalled();

  Range scheme_range() const { return scheme_range_; }
  Range emphasis_range() const { return emphasis_range_; }
  bool base_text_emphasis() const { return base_text_emphasis_; }

  // Returns the latest color applied to |range| via ApplyColor(), or
  // std::nullopt if no color has been applied to |range|.
  std::optional<SkColor> GetLatestColorForRange(const gfx::Range& range);

  // Returns the latest style applied to |range| via ApplyStyle(), or
  // std::nullopt if no color has been applied to |range|.
  std::optional<std::pair<gfx::TextStyle, bool>> GetLatestStyleForRange(
      const gfx::Range& range) const;

  // Resets the captured styles.
  void ResetStyles();

  // OmniboxViewViews:
  void OnThemeChanged() override;

  using OmniboxView::OnInlineAutocompleteTextMaybeChanged;

  using OmniboxViewViews::SetTextAndSelectedRanges;
  using OmniboxViewViews::SkipDefaultKeyEventProcessing;

 protected:
  // OmniboxViewViews:
  void UpdateSchemeStyle(const Range& range) override;

 private:
  // OmniboxViewViews:
  // There is no popup and it doesn't actually matter whether we change the
  // visual style of the text, so these methods are all overridden merely to
  // capture relevant state at the time of the call, to be checked by test code.
  void UpdatePopup() override;
  void SetEmphasis(bool emphasize, const Range& range) override;
  void ApplyColor(SkColor color, const gfx::Range& range) override;
  void ApplyStyle(gfx::TextStyle style,
                  bool value,
                  const gfx::Range& range) override;

  size_t update_popup_call_count_ = 0;
  std::u16string update_popup_text_;
  Range update_popup_selection_range_;

  // Range of the last scheme logged by UpdateSchemeStyle().
  Range scheme_range_;

  // Range of the last text emphasized by SetEmphasis().
  Range emphasis_range_;

  // Stores the colors applied to ranges via ApplyColor(), in chronological
  // order.
  std::vector<std::pair<SkColor, gfx::Range>> range_colors_;

  // Stores the styles applied to ranges via ApplyStyle(), in chronological
  // order.
  std::vector<std::tuple<gfx::TextStyle, bool, gfx::Range>> range_styles_;

  // SetEmphasis() logs whether the base color of the text is emphasized.
  bool base_text_emphasis_;
};

TestingOmniboxView::TestingOmniboxView(std::unique_ptr<OmniboxClient> client,
                                       bool popup_window_mode)
    : OmniboxViewViews(std::move(client),
                       popup_window_mode,
                       nullptr,
                       gfx::FontList()) {}

void TestingOmniboxView::ResetEmphasisTestState() {
  base_text_emphasis_ = false;
  emphasis_range_ = Range::InvalidRange();
  scheme_range_ = Range::InvalidRange();
}

void TestingOmniboxView::CheckUpdatePopupCallInfo(
    size_t call_count,
    std::u16string_view text,
    const Range& selection_range) {
  EXPECT_EQ(call_count, update_popup_call_count_);
  EXPECT_EQ(text, update_popup_text_);
  EXPECT_EQ(selection_range, update_popup_selection_range_);
}

void TestingOmniboxView::CheckUpdatePopupNotCalled() {
  EXPECT_EQ(update_popup_call_count_, 0U);
}

std::optional<SkColor> TestingOmniboxView::GetLatestColorForRange(
    const gfx::Range& range) {
  // Iterate backwards to get the most recently applied color for |range|.
  for (const auto& [color, other_range] : base::Reversed(range_colors_)) {
    if (range == other_range)
      return color;
  }
  return std::nullopt;
}

std::optional<std::pair<gfx::TextStyle, bool>>
TestingOmniboxView::GetLatestStyleForRange(const gfx::Range& range) const {
  // Iterate backwards to get the most recently applied style for |range|.
  for (const auto& [style, value, other_range] :
       base::Reversed(range_styles_)) {
    if (range == other_range)
      return std::make_pair(style, value);
  }
  return std::nullopt;
}

void TestingOmniboxView::ResetStyles() {
  range_styles_.clear();
}

void TestingOmniboxView::OnThemeChanged() {
  // This method is overridden simply to expose this protected method for tests
  // to call.
  OmniboxViewViews::OnThemeChanged();
}

void TestingOmniboxView::UpdateSchemeStyle(const Range& range) {
  scheme_range_ = range;
  OmniboxViewViews::UpdateSchemeStyle(range);
}

void TestingOmniboxView::UpdatePopup() {
  ++update_popup_call_count_;
  update_popup_text_ = GetText();
  update_popup_selection_range_ = GetSelectedRange();

  // The real view calls OmniboxEditModel::UpdateInput(), which sets input in
  // progress and starts autocomplete.  Triggering autocomplete and the popup is
  // beyond the scope of this test, but setting input in progress is important
  // for making some sequences (e.g. uneliding on taking an action) behave
  // correctly.
  model()->SetInputInProgress(true);
}

void TestingOmniboxView::SetEmphasis(bool emphasize, const Range& range) {
  if (range == Range::InvalidRange()) {
    base_text_emphasis_ = emphasize;
    return;
  }

  EXPECT_TRUE(emphasize);
  emphasis_range_ = range;
}

void TestingOmniboxView::ApplyColor(SkColor color, const gfx::Range& range) {
  range_colors_.emplace_back(color, range);
  OmniboxViewViews::ApplyColor(color, range);
}

void TestingOmniboxView::ApplyStyle(gfx::TextStyle style,
                                    bool value,
                                    const gfx::Range& range) {
  range_styles_.emplace_back(style, value, range);
  OmniboxViewViews::ApplyStyle(style, value, range);
}

// TestLocationBar -------------------------------------------------------------

class TestLocationBar : public LocationBar {
 public:
  TestLocationBar(CommandUpdater* command_updater,
                  LocationBarModel* location_bar_model)
      : LocationBar(command_updater), location_bar_model_(location_bar_model) {}
  TestLocationBar(const TestLocationBar&) = delete;
  TestLocationBar& operator=(const TestLocationBar&) = delete;
  ~TestLocationBar() override = default;

  void set_omnibox_view(OmniboxViewViews* view) { omnibox_view_ = view; }

  // LocationBar:
  void FocusLocation(bool select_all) override {}
  void FocusSearch() override {}
  void UpdateContentSettingsIcons() override {}
  void SaveStateToContents(content::WebContents* contents) override {}
  void Revert() override {}
  OmniboxView* GetOmniboxView() override { return nullptr; }
  LocationBarTesting* GetLocationBarForTesting() override { return nullptr; }
  LocationBarModel* GetLocationBarModel() override {
    return location_bar_model_;
  }
  content::WebContents* GetWebContents() override { return nullptr; }
  void OnChanged() override {}
  void OnPopupVisibilityChanged() override {}
  void UpdateWithoutTabRestore() override {
    // This is a minimal amount of what LocationBarView does. Not all tests
    // set |omnibox_view_|.
    if (omnibox_view_) {
      omnibox_view_->Update();
    }
  }

  raw_ptr<LocationBarModel> location_bar_model_;
  raw_ptr<OmniboxViewViews> omnibox_view_ = nullptr;
};

// OmniboxViewViewsTest -------------------------------------------------------

// Base class that ensures ScopedFeatureList is initialized first.
class OmniboxViewViewsTestBase : public ChromeViewsTestBase {
 public:
  OmniboxViewViewsTestBase(
      const std::vector<base::test::FeatureRefAndParams>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features,
      bool is_rtl_ui_test = false) {
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
    base::i18n::SetRTLForTesting(is_rtl_ui_test);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class OmniboxViewViewsTest : public OmniboxViewViewsTestBase {
 public:
  OmniboxViewViewsTest(
      const std::vector<base::test::FeatureRefAndParams>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features,
      bool is_rtl_ui_test = false,
      bool is_popup_window_mode = false);

  OmniboxViewViewsTest()
      : OmniboxViewViewsTest(std::vector<base::test::FeatureRefAndParams>(),
                             std::vector<base::test::FeatureRef>()) {}
  OmniboxViewViewsTest(const OmniboxViewViewsTest&) = delete;
  OmniboxViewViewsTest& operator=(const OmniboxViewViewsTest&) = delete;

  TestLocationBarModel* location_bar_model() { return &location_bar_model_; }
  CommandUpdaterImpl* command_updater() { return &command_updater_; }
  TestingOmniboxView* omnibox_view() const { return omnibox_view_; }

  // TODO(tommycli): These base class accessors exist because Textfield and
  // OmniboxView both hide member functions that were public in base classes.
  // Remove these after we stop doing that.
  views::Textfield* omnibox_textfield() const { return omnibox_view(); }
  views::View* omnibox_textfield_view() const { return omnibox_view(); }

  views::TextfieldTestApi GetTextfieldTestApi() {
    return views::TextfieldTestApi(omnibox_view());
  }

  // Sets |new_text| as the omnibox text, and emphasizes it appropriately.  If
  // |accept_input| is true, pretends that the user has accepted this input
  // (i.e. it's been navigated to).
  void SetAndEmphasizeText(const std::string& new_text, bool accept_input);

  bool IsCursorEnabled() {
    return GetTextfieldTestApi().GetRenderText()->cursor_enabled();
  }

  ui::MouseEvent CreateMouseEvent(ui::EventType type,
                                  const gfx::Point& point,
                                  int event_flags = ui::EF_LEFT_MOUSE_BUTTON) {
    return ui::MouseEvent(type, point, point, ui::EventTimeForNow(),
                          event_flags, event_flags);
  }

 protected:
  Browser* browser() { return browser_.get(); }
  Profile* profile() { return profile_.get(); }
  TestLocationBar* location_bar() { return &location_bar_; }

  // Updates the models' URL and display text to |new_url|.
  void UpdateDisplayURL(std::u16string_view new_url) {
    location_bar_model()->set_url(GURL(new_url));
    location_bar_model()->set_url_for_display(std::u16string(new_url));
    omnibox_view()->model()->ResetDisplayTexts();
    omnibox_view()->RevertAll();
  }

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  ui::MouseEvent CreateEvent(ui::EventType type, int flags) {
    return ui::MouseEvent(type, gfx::Point(0, 0), gfx::Point(),
                          ui::EventTimeForNow(), flags, 0);
  }

  bool is_popup_window_mode_ = false;

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TemplateURLServiceFactoryTestUtil> util_;
  CommandUpdaterImpl command_updater_;
  TestLocationBarModel location_bar_model_;
  TestLocationBar location_bar_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;

  std::unique_ptr<views::Widget> widget_;

  // Owned by |widget_|.
  raw_ptr<TestingOmniboxView> omnibox_view_ = nullptr;
};

OmniboxViewViewsTest::OmniboxViewViewsTest(
    const std::vector<base::test::FeatureRefAndParams>& enabled_features,
    const std::vector<base::test::FeatureRef>& disabled_features,
    bool is_rtl_ui_test,
    bool is_popup_window_mode)
    : OmniboxViewViewsTestBase(enabled_features,
                               disabled_features,
                               is_rtl_ui_test),
      is_popup_window_mode_(is_popup_window_mode),
      command_updater_(nullptr),
      location_bar_(&command_updater_, &location_bar_model_) {}

void OmniboxViewViewsTest::SetAndEmphasizeText(const std::string& new_text,
                                               bool accept_input) {
  omnibox_view()->ResetEmphasisTestState();
  omnibox_view()->SetUserText(base::ASCIIToUTF16(new_text));
  if (accept_input) {
    // We don't need to actually navigate in this case (and doing so in a test
    // would be difficult); it's sufficient to mark input as "no longer in
    // progress", and the edit model will assume the current text is a URL.
    omnibox_view()->model()->SetInputInProgress(false);
  }
  omnibox_view()->EmphasizeURLComponents();
}

void OmniboxViewViewsTest::SetUp() {
  ChromeViewsTestBase::SetUp();

  // We need the signin client initialized with a TestURLLoaderFactory.
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(
      ChromeSigninClientFactory::GetInstance(),
      base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                          &test_url_loader_factory_));
  profile_ = profile_builder.Build();
  browser_window_ = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), /*user_gesture*/ true);
  params.type = Browser::TYPE_NORMAL;
  params.window = browser_window_.get();
  browser_.reset(Browser::Create(params));

  util_ = std::make_unique<TemplateURLServiceFactoryTestUtil>(profile_.get());

  // We need a widget so OmniboxView can be correctly focused and unfocused.
  widget_ =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  widget_->Show();

  AutocompleteClassifierFactory::GetInstance()->SetTestingFactoryAndUse(
      profile_.get(),
      base::BindRepeating(&AutocompleteClassifierFactory::BuildInstanceFor));
  auto omnibox_view = std::make_unique<TestingOmniboxView>(
      std::make_unique<ChromeOmniboxClient>(&location_bar_, browser(),
                                            profile()),
      is_popup_window_mode_);
  omnibox_view->Init();

  omnibox_view_ = widget_->SetContentsView(std::move(omnibox_view));
}

void OmniboxViewViewsTest::TearDown() {
  // Clean ourselves up as the text input client.
  if (omnibox_view_->GetInputMethod())
    omnibox_view_->GetInputMethod()->DetachTextInputClient(omnibox_view_);

  location_bar()->set_omnibox_view(nullptr);
  omnibox_view_ = nullptr;
  browser_->tab_strip_model()->CloseAllTabs();
  browser_ = nullptr;
  browser_window_ = nullptr;

  widget_.reset();
  util_.reset();
  profile_.reset();

  ChromeViewsTestBase::TearDown();
}

class OmniboxViewViewsTestIsPopupWindowMode : public OmniboxViewViewsTest {
 public:
  OmniboxViewViewsTestIsPopupWindowMode()
      : OmniboxViewViewsTest(/* enabled_features */ {},
                             /* disabled_features */ {},
                             /* is_rtl_ui_test */ false,
                             /* is_popup_window_mode */ true) {}
};

// Actual tests ---------------------------------------------------------------

// Checks that a single change of the text in the omnibox invokes
// only one call to OmniboxViewViews::UpdatePopup().
TEST_F(OmniboxViewViewsTest, UpdatePopupCall) {
  ui::KeyEvent char_event(ui::EventType::kKeyPressed, ui::VKEY_A,
                          ui::DomCode::US_A, 0, ui::DomKey::FromCharacter('a'),
                          ui::EventTimeForNow());
  omnibox_textfield()->InsertChar(char_event);
  omnibox_view()->CheckUpdatePopupCallInfo(1, u"a", Range(1));

  char_event =
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_B, ui::DomCode::US_B, 0,
                   ui::DomKey::FromCharacter('b'), ui::EventTimeForNow());
  omnibox_textfield()->InsertChar(char_event);
  omnibox_view()->CheckUpdatePopupCallInfo(2, u"ab", Range(2));

  ui::KeyEvent pressed(ui::EventType::kKeyPressed, ui::VKEY_BACK, 0);
  omnibox_textfield()->OnKeyEvent(&pressed);
  omnibox_view()->CheckUpdatePopupCallInfo(3, u"a", Range(1));
}

// Test that text cursor is shown in the omnibox after entering any single
// character in NTP 'Search box'. Test for crbug.com/698172.
TEST_F(OmniboxViewViewsTest, EditTextfield) {
  omnibox_textfield()->SetCursorEnabled(false);
  ui::KeyEvent char_event(ui::EventType::kKeyPressed, ui::VKEY_A,
                          ui::DomCode::US_A, 0, ui::DomKey::FromCharacter('a'),
                          ui::EventTimeForNow());
  omnibox_textfield()->InsertChar(char_event);
  EXPECT_TRUE(IsCursorEnabled());
}

// Test that the scheduled text edit command is cleared when Textfield receives
// a key press event. This ensures that the scheduled text edit command property
// is always in the correct state. Test for http://crbug.com/613948.
TEST_F(OmniboxViewViewsTest, ScheduledTextEditCommand) {
  omnibox_textfield()->SetTextEditCommandForNextKeyEvent(
      ui::TextEditCommand::MOVE_UP);
  EXPECT_EQ(ui::TextEditCommand::MOVE_UP,
            GetTextfieldTestApi().scheduled_text_edit_command());

  ui::KeyEvent up_pressed(ui::EventType::kKeyPressed, ui::VKEY_UP, 0);
  omnibox_textfield()->OnKeyEvent(&up_pressed);
  EXPECT_EQ(ui::TextEditCommand::INVALID_COMMAND,
            GetTextfieldTestApi().scheduled_text_edit_command());
}

// Test that Shift+Up and Shift+Down are not captured and let selection mode
// take over. Test for crbug.com/863543 and crbug.com/892216.
TEST_F(OmniboxViewViewsTest, SelectWithShift_863543) {
  location_bar_model()->set_url(GURL("http://www.example.com/?query=1"));
  const std::u16string text = u"http://www.example.com/?query=1";
  omnibox_view()->SetWindowTextAndCaretPos(text, 23U, false, false);

  ui::KeyEvent shift_up_pressed(ui::EventType::kKeyPressed, ui::VKEY_UP,
                                ui::EF_SHIFT_DOWN);
  omnibox_textfield()->OnKeyEvent(&shift_up_pressed);

  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(23U, start);
  EXPECT_EQ(0U, end);
  omnibox_view()->CheckUpdatePopupNotCalled();

  omnibox_view()->SetWindowTextAndCaretPos(text, 18U, false, false);

  ui::KeyEvent shift_down_pressed(ui::EventType::kKeyPressed, ui::VKEY_DOWN,
                                  ui::EF_SHIFT_DOWN);
  omnibox_textfield()->OnKeyEvent(&shift_down_pressed);

  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(18U, start);
  EXPECT_EQ(31U, end);
  omnibox_view()->CheckUpdatePopupNotCalled();
}

TEST_F(OmniboxViewViewsTest, OnBlur) {
  // Make the Omnibox very narrow (so it couldn't fit the whole string).
  int kOmniboxWidth = 60;
  gfx::RenderText* render_text = omnibox_view()->GetRenderText();
  render_text->SetDisplayRect(gfx::Rect(0, 0, kOmniboxWidth, 10));
  render_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // (In this example, uppercase Latin letters represent Hebrew letters.)
  // The string |kContentsRtl| is equivalent to:
  //     RA.QWM/0123/abcd
  // This is displayed as:
  //     0123/MWQ.AR/abcd
  // Enter focused mode, where the text should *not* be elided, and we expect
  // SetWindowTextAndCaretPos to scroll such that the start of the string is
  // on-screen. Because the domain is RTL, this scrolls to an offset greater
  // than 0.
  omnibox_textfield()->OnFocus();
  static constexpr std::u16string_view kContentsRtl =
      u"\x05e8\x05e2.\x05e7\x05d5\x05dd/0123/abcd";
  omnibox_view()->SetWindowTextAndCaretPos(std::u16string(kContentsRtl), 0,
                                           false, false);
  EXPECT_EQ(gfx::NO_ELIDE, render_text->elide_behavior());

  // TODO(crbug.com/40699469): this assertion fails because
  // EmphasizeURLComponents() sets the textfield's directionality to
  // DIRECTIONALITY_AS_URL. This should be either fixed or the assertion
  // removed.
  //
  // NOTE: Technically (depending on the font), this expectation could fail if
  // the entire domain fits in 60 pixels. However, 60px is so small it should
  // never happen with any font.
  // EXPECT_GT(0, render_text->GetUpdatedDisplayOffset().x());

  omnibox_view()->SelectAll(false);
  EXPECT_TRUE(omnibox_view()->IsSelectAll());

  // Now enter blurred mode, where the text should be elided to 60px. This means
  // the string itself is truncated. Scrolling would therefore mean the text is
  // off-screen. Ensure that the horizontal scrolling has been reset to 0.
  omnibox_textfield()->OnBlur();
  EXPECT_EQ(gfx::ELIDE_TAIL, render_text->elide_behavior());
  EXPECT_EQ(0, render_text->GetUpdatedDisplayOffset().x());
  EXPECT_FALSE(omnibox_view()->IsSelectAll());
}

// Verifies that https://crbug.com/45260 doesn't regress.
TEST_F(OmniboxViewViewsTest,
       RendererInitiatedFocusSelectsAllWhenStartingBlurred) {
  location_bar_model()->set_url(GURL("about:blank"));
  omnibox_view()->model()->ResetDisplayTexts();
  omnibox_view()->RevertAll();

  // Simulate a renderer-initated focus event. Expect that everything is
  // selected now.
  omnibox_view()->SetFocus(/*is_user_initiated=*/false);
  EXPECT_TRUE(omnibox_view()->IsSelectAll());
}

// Verifies that https://crbug.com/924935 doesn't regress.
TEST_F(OmniboxViewViewsTest,
       RendererInitiatedFocusPreservesCursorWhenStartingFocused) {
  // Simulate the user focusing the omnibox and typing something. This is just
  // the test setup, not the actual focus event we are testing.
  omnibox_view()->SetFocus(/*is_user_initiated*/ true);
  omnibox_view()->SetTextAndSelectedRanges(u"user text", {gfx::Range(9, 9)});
  ASSERT_FALSE(omnibox_view()->IsSelectAll());
  ASSERT_TRUE(omnibox_view()->GetSelectionAtEnd());

  // Simulate a renderer-initated focus event. Expect the cursor position to be
  // preserved, and that the omnibox did not select-all the text.
  omnibox_view()->SetFocus(/*is_user_initiated=*/false);
  EXPECT_FALSE(omnibox_view()->IsSelectAll());
  EXPECT_TRUE(omnibox_view()->GetSelectionAtEnd());
}

TEST_F(OmniboxViewViewsTest, Emphasis) {
  constexpr struct {
    const char* input;
    bool expected_base_text_emphasized;
    Range expected_emphasis_range;
    Range expected_scheme_range;
  } test_cases[] = {
      {"data:text/html,Hello%20World", false, Range(0, 4), Range(0, 4)},
      {"http://www.example.com/path/file.htm", false, Range(7, 22),
       Range(0, 4)},
      {"https://www.example.com/path/file.htm", false, Range(8, 23),
       Range(0, 5)},
      {"chrome-extension://ldfbacdbackkjhclmhnjabngnppnkagl", false,
       Range::InvalidRange(), Range(0, 16)},
      {"nosuchscheme://opaque/string", true, Range::InvalidRange(),
       Range(0, 12)},
      {"nosuchscheme:opaquestring", true, Range::InvalidRange(), Range(0, 12)},
      {"host.com/path/file", false, Range(0, 8), Range::InvalidRange()},
      {"This is plain text", true, Range::InvalidRange(),
       Range::InvalidRange()},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.input);

    SetAndEmphasizeText(test_case.input, false);
    EXPECT_EQ(test_case.expected_base_text_emphasized,
              omnibox_view()->base_text_emphasis());
    EXPECT_EQ(test_case.expected_emphasis_range,
              omnibox_view()->emphasis_range());
    EXPECT_FALSE(omnibox_view()->scheme_range().IsValid());

    if (test_case.expected_scheme_range.IsValid()) {
      SetAndEmphasizeText(test_case.input, true);
      EXPECT_EQ(test_case.expected_base_text_emphasized,
                omnibox_view()->base_text_emphasis());
      EXPECT_EQ(test_case.expected_emphasis_range,
                omnibox_view()->emphasis_range());
      EXPECT_EQ(test_case.expected_scheme_range,
                omnibox_view()->scheme_range());
    }
  }
}

TEST_F(OmniboxViewViewsTest, RevertOnBlur) {
  location_bar_model()->set_url(GURL("https://example.com/"));
  omnibox_view()->model()->ResetDisplayTexts();
  omnibox_view()->RevertAll();

  EXPECT_EQ(u"https://example.com/", omnibox_view()->GetText());
  EXPECT_FALSE(omnibox_view()->model()->user_input_in_progress());

  // Set the view text without updating the model's user text. This usually
  // occurs when the omnibox unapplies Steady State Elisions to temporarily show
  // the full URL to the user.
  omnibox_view()->SetWindowTextAndCaretPos(u"view text", 0, false, false);
  EXPECT_EQ(u"view text", omnibox_view()->GetText());
  EXPECT_FALSE(omnibox_view()->model()->user_input_in_progress());

  // Expect that on blur, we revert to the original text and are not in user
  // input mode.
  omnibox_textfield()->OnBlur();
  EXPECT_EQ(u"https://example.com/", omnibox_view()->GetText());
  EXPECT_FALSE(omnibox_view()->model()->user_input_in_progress());

  // Now set user text, which is reflected into the model as well.
  omnibox_view()->SetUserText(u"user text");
  EXPECT_EQ(u"user text", omnibox_view()->GetText());
  EXPECT_TRUE(omnibox_view()->model()->user_input_in_progress());

  // Expect that on blur, if the text has been edited, stay in user input mode.
  omnibox_textfield()->OnBlur();
  EXPECT_EQ(u"user text", omnibox_view()->GetText());
  EXPECT_TRUE(omnibox_view()->model()->user_input_in_progress());
}

TEST_F(OmniboxViewViewsTest, RevertOnEscape) {
  location_bar_model()->set_url(GURL("https://permanent-text.com/"));
  omnibox_view()->model()->ResetDisplayTexts();
  omnibox_view()->RevertAll();

  EXPECT_EQ(u"https://permanent-text.com/", omnibox_view()->GetText());
  EXPECT_FALSE(omnibox_view()->model()->user_input_in_progress());

  omnibox_view()->SetUserText(u"user text");
  EXPECT_EQ(u"user text", omnibox_view()->GetText());
  EXPECT_TRUE(omnibox_view()->model()->user_input_in_progress());

  // Expect that on Escape, the text is reverted to the permanent URL.
  ui::KeyEvent escape(ui::EventType::kKeyPressed, ui::VKEY_ESCAPE, 0);
  omnibox_textfield()->OnKeyEvent(&escape);

  EXPECT_EQ(u"https://permanent-text.com/", omnibox_view()->GetText());
  EXPECT_FALSE(omnibox_view()->model()->user_input_in_progress());
}

TEST_F(OmniboxViewViewsTest, ShiftEscapeDoesNotSkipDefaultProcessing) {
  ui::KeyEvent shiftEscape(ui::EventType::kKeyPressed, ui::VKEY_ESCAPE,
                           ui::EF_SHIFT_DOWN);
  EXPECT_EQ(omnibox_view()->SkipDefaultKeyEventProcessing(shiftEscape), false);
}

TEST_F(OmniboxViewViewsTest, EscapeSkipsDefaultProcessing) {
  ui::KeyEvent escape(ui::EventType::kKeyPressed, ui::VKEY_ESCAPE, 0);
  EXPECT_EQ(omnibox_view()->SkipDefaultKeyEventProcessing(escape), true);
}

TEST_F(OmniboxViewViewsTest, BackspaceExitsKeywordMode) {
  omnibox_view()->SetUserText(u"user text");
  omnibox_view()->model()->EnterKeywordModeForDefaultSearchProvider(
      OmniboxEventProto::KEYBOARD_SHORTCUT);

  ASSERT_EQ(u"user text", omnibox_view()->GetText());
  ASSERT_TRUE(omnibox_view()->IsSelectAll());
  ASSERT_FALSE(omnibox_view()->model()->keyword().empty());

  // First backspace should clear the user text but not exit keyword mode.
  ui::KeyEvent backspace(ui::EventType::kKeyPressed, ui::VKEY_BACK, 0);
  omnibox_textfield()->OnKeyEvent(&backspace);
  EXPECT_TRUE(omnibox_view()->GetText().empty());
  EXPECT_FALSE(omnibox_view()->model()->keyword().empty());

  // Second backspace should exit keyword mode.
  omnibox_textfield()->OnKeyEvent(&backspace);
  EXPECT_TRUE(omnibox_view()->GetText().empty());
  EXPECT_TRUE(omnibox_view()->model()->keyword().empty());
}

TEST_F(OmniboxViewViewsTest, BlurNeverExitsKeywordMode) {
  location_bar_model()->set_url(GURL());
  omnibox_view()->model()->ResetDisplayTexts();
  omnibox_view()->RevertAll();

  // Enter keyword mode, but with no user text.
  omnibox_view()->model()->EnterKeywordModeForDefaultSearchProvider(
      OmniboxEventProto::KEYBOARD_SHORTCUT);
  EXPECT_TRUE(omnibox_view()->GetText().empty());
  EXPECT_FALSE(omnibox_view()->model()->keyword().empty());

  // Expect that on blur, stay in keyword mode.
  omnibox_textfield()->OnBlur();
  EXPECT_TRUE(omnibox_view()->GetText().empty());
  EXPECT_FALSE(omnibox_view()->model()->keyword().empty());
}

TEST_F(OmniboxViewViewsTest, PasteAndGoToUrlOrSearchCommand) {
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  ui::ClipboardBuffer clipboard_buffer = ui::ClipboardBuffer::kCopyPaste;
  command_updater()->UpdateCommandEnabled(IDC_OPEN_CURRENT_URL, true);

  // Test command is disabled for an empty clipboard.
  clipboard->Clear(clipboard_buffer);
  EXPECT_FALSE(omnibox_view()->IsCommandIdEnabled(IDC_PASTE_AND_GO));

  // Test input that's a valid URL.
  std::u16string expected_text =
#if BUILDFLAG(IS_MAC)
      u"Pa&ste and Go to https://test.com";
#else
      u"Pa&ste and go to https://test.com";
#endif
  ui::ScopedClipboardWriter(clipboard_buffer).WriteText(u"https://test.com/");
  std::u16string returned_text =
      omnibox_view()->GetLabelForCommandId(IDC_PASTE_AND_GO);
  EXPECT_TRUE(omnibox_view()->IsCommandIdEnabled(IDC_PASTE_AND_GO));
  EXPECT_EQ(expected_text, returned_text);

  // Test input that's URL-like. (crbug.com/980002).
  expected_text =
#if BUILDFLAG(IS_MAC)
      u"Pa&ste and Go to test.com";
#else
      u"Pa&ste and go to test.com";
#endif
  ui::ScopedClipboardWriter(clipboard_buffer).WriteText(u"test.com");
  returned_text = omnibox_view()->GetLabelForCommandId(IDC_PASTE_AND_GO);
  EXPECT_TRUE(omnibox_view()->IsCommandIdEnabled(IDC_PASTE_AND_GO));
  EXPECT_EQ(expected_text, returned_text);

  // Test input that's search-like.
  expected_text =
#if BUILDFLAG(IS_MAC)
      u"Pa&ste and Search for \x201Cthis is a test sentence\x201D";
#else
      u"Pa&ste and search for \x201Cthis is a test sentence\x201D";
#endif
  ui::ScopedClipboardWriter(clipboard_buffer)
      .WriteText(u"this is a test sentence");
  returned_text = omnibox_view()->GetLabelForCommandId(IDC_PASTE_AND_GO);
  EXPECT_TRUE(omnibox_view()->IsCommandIdEnabled(IDC_PASTE_AND_GO));
  EXPECT_EQ(expected_text, returned_text);
}

TEST_F(OmniboxViewViewsTest, SelectAllCommand) {
  omnibox_view()->SetUserText(u"user text");
  EXPECT_TRUE(omnibox_view()->IsCommandIdEnabled(views::Textfield::kSelectAll));

  omnibox_view()->ExecuteCommand(views::Textfield::kSelectAll, 0);
  EXPECT_TRUE(omnibox_view()->IsSelectAll());
  // Test command is disabled if text is already all selected.
  EXPECT_FALSE(
      omnibox_view()->IsCommandIdEnabled(views::Textfield::kSelectAll));
}

// Verifies |OmniboxEditModel::State::needs_revert_and_select_all|, and verifies
// a recent regression in this logic (see https://crbug.com/923290).
TEST_F(OmniboxViewViewsTest, SelectAllOnReactivateTabAfterDeleteAll) {
  location_bar()->set_omnibox_view(omnibox_view());

  auto web_contents1 =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);

  // Simulate a new tab with "about:blank".
  const GURL url_1("about:blank/");
  location_bar_model()->set_url(url_1);
  omnibox_view()->model()->ResetDisplayTexts();
  omnibox_view()->RevertAll();
  omnibox_view()->SaveStateToTab(web_contents1.get());

  // Simulate creating another tab at "chrome://history". The second url should
  // be longer than the first (to trigger the bug).
  auto web_contents2 =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  const GURL url_2("chrome://history/");
  EXPECT_GT(url_2.spec().size(), url_1.spec().size());
  // Notice the url is set before ResetDisplayTexts(), this matches what
  // actually happens in code.
  location_bar_model()->set_url(url_2);
  omnibox_view()->model()->ResetDisplayTexts();
  omnibox_view()->RevertAll();

  // Delete all the text.
  omnibox_view()->SetUserText(std::u16string());
  EXPECT_TRUE(omnibox_view()->model()->user_input_in_progress());

  // Switch back to the first url.
  location_bar_model()->set_url(url_1);
  omnibox_view()->SaveStateToTab(web_contents2.get());
  omnibox_view()->OnTabChanged(web_contents1.get());

  // Switch back to the second url. Even though the text was deleted earlier,
  // the previous text (|url_2|) should be restored *and* all the text selected.
  location_bar_model()->set_url(url_2);
  omnibox_view()->SaveStateToTab(web_contents1.get());
  omnibox_view()->OnTabChanged(web_contents2.get());
  EXPECT_EQ(url_2, GURL(base::UTF16ToUTF8(omnibox_view()->GetText())));
  EXPECT_TRUE(omnibox_view()->IsSelectAll());
}

TEST_F(OmniboxViewViewsTest, SelectAllDuringMouseDown) {
  omnibox_textfield()->OnMousePressed(
      CreateMouseEvent(ui::EventType::kMousePressed, {0, 0}));
  omnibox_view()->SetUserText(u"abc");
  ui::KeyEvent event_a(ui::EventType::kKeyPressed, ui::VKEY_A, 0);
  EXPECT_FALSE(omnibox_view()->IsSelectAll());
  omnibox_textfield_view()->OnKeyPressed(event_a);
  // Normally SelectAll happens after OnMouseRelease. Verifying this happens
  // during OnKeyPress when the mouse is down.
  EXPECT_TRUE(omnibox_view()->IsSelectAll());
}

TEST_F(OmniboxViewViewsTest, SetWindowTextAndCaretPos) {
  // googl|e.com
  omnibox_view()->SetWindowTextAndCaretPos(u"google.com", 5, false, false);
  EXPECT_EQ(u"google.com", omnibox_view()->GetText());
  EXPECT_EQ(omnibox_view()->GetRenderText()->GetAllSelections(),
            (std::vector<Range>{{5, 5}}));
}

TEST_F(OmniboxViewViewsTest, OnInlineAutocompleteTextMaybeChanged) {
  // No selection, google.com|
  omnibox_view()->OnInlineAutocompleteTextMaybeChanged(u"google.com",
                                                       {{10, 10}}, u"", u"");
  EXPECT_EQ(u"google.com", omnibox_view()->GetText());
  EXPECT_EQ(omnibox_view()->GetRenderText()->GetAllSelections(),
            (std::vector<Range>{{10, 10}}));

  // Single selection, gmai[l.com]
  omnibox_view()->OnInlineAutocompleteTextMaybeChanged(u"gmail.com", {{9, 4}},
                                                       u"", u"l.com");
  EXPECT_EQ(u"gmail.com", omnibox_view()->GetText());
  EXPECT_EQ(omnibox_view()->GetRenderText()->GetAllSelections(),
            (std::vector<Range>{{9, 4}}));

  // Multiselection, [go]ogl[e.com]
  omnibox_view()->OnInlineAutocompleteTextMaybeChanged(
      u"google.com", {{10, 5}, {0, 2}}, u"go", u"e.com");
  EXPECT_EQ(u"google.com", omnibox_view()->GetText());
  EXPECT_EQ(omnibox_view()->GetRenderText()->GetAllSelections(),
            (std::vector<Range>{{10, 5}, {0, 2}}));
}

TEST_F(OmniboxViewViewsTest, OverflowingAutocompleteText) {
  // Make the Omnibox narrow so it can't fit the entire string (~650px), but
  // wide enough to fit the user text (~65px).
  int kOmniboxWidth = 100;
  gfx::RenderText* render_text = omnibox_view()->GetRenderText();
  render_text->SetDisplayRect(gfx::Rect(0, 0, kOmniboxWidth, 10));

  omnibox_textfield()->OnFocus();
  omnibox_view()->OnInlineAutocompleteTextMaybeChanged(
      u"user text. Followed by very long autocompleted text that is unlikely "
      u"to fit in |kOmniboxWidth|",
      {{94, 10}}, u"",
      u" Followed by very long autocompleted text that is unlikely to fit in "
      u"|kOmniboxWidth|");

  // NOTE: Technically (depending on the font), this expectation could fail if
  // 'user text' doesn't fit in 100px or the entire string fits in 100px.
  EXPECT_EQ(render_text->GetUpdatedDisplayOffset().x(), 0);
  EXPECT_FALSE(omnibox_view()->IsSelectAll());

  // On blur, the display should remain to the start of the text.
  omnibox_textfield()->OnBlur();
  EXPECT_EQ(render_text->GetUpdatedDisplayOffset().x(), 0);
  EXPECT_FALSE(omnibox_view()->IsSelectAll());
}

TEST_F(OmniboxViewViewsTest, SchemeStrikethrough) {
  constexpr gfx::Range kSchemeRange(0, 5);
  location_bar_model()->set_url(GURL("https://test.com/"));
  omnibox_view()->model()->ResetDisplayTexts();
  omnibox_view()->ResetStyles();

  // Strikethrough should not be keyed off the security state.
  location_bar_model()->set_security_level(security_state::DANGEROUS);
  omnibox_view()->RevertAll();
  auto style = omnibox_view()->GetLatestStyleForRange(kSchemeRange);
  EXPECT_FALSE(style.has_value());
  omnibox_view()->ResetStyles();

  // Rather, it should be keyed off the cert status.
  location_bar_model()->set_cert_status(net::CERT_STATUS_REVOKED);
  omnibox_view()->RevertAll();
  style = omnibox_view()->GetLatestStyleForRange(kSchemeRange);
  ASSERT_TRUE(style.has_value());
  EXPECT_EQ(style.value(), std::make_pair(gfx::TEXT_STYLE_STRIKE, true));
  omnibox_view()->ResetStyles();

  location_bar_model()->set_security_level(security_state::NONE);
  omnibox_view()->RevertAll();
  style = omnibox_view()->GetLatestStyleForRange(kSchemeRange);
  ASSERT_TRUE(style.has_value());
  EXPECT_EQ(style.value(), std::make_pair(gfx::TEXT_STYLE_STRIKE, true));
  omnibox_view()->ResetStyles();

  location_bar_model()->set_cert_status(0);
  omnibox_view()->RevertAll();
  style = omnibox_view()->GetLatestStyleForRange(kSchemeRange);
  EXPECT_FALSE(style.has_value());
}

#if BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)
TEST_F(OmniboxViewViewsTest,
       AccessibleTextOffsetsUpdatesAfterElideBehaviorChange) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(::features::kUiaProvider);

  // Make the Omnibox very narrow (so it couldn't fit the whole string).
  int kOmniboxWidth = 60;
  gfx::RenderText* render_text = omnibox_view()->GetRenderText();
  render_text->SetDisplayRect(gfx::Rect(0, 0, kOmniboxWidth, 10));
  render_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  const std::u16string text = u"http://www.example.com/?query=1";
  omnibox_view()->SetWindowTextAndCaretPos(text, 23U, false, false);

  EXPECT_EQ(gfx::ELIDE_TAIL, render_text->elide_behavior());
  ui::AXNodeData node_data;
  omnibox_view()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  std::vector<int32_t> expected_offsets = {
      0,  6,  10, 14, 21, 24, 29, 33, 42, 52, 52, 52, 52, 52, 52, 52,
      52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52};
  EXPECT_EQ(node_data.GetIntListAttribute(
                ax::mojom::IntListAttribute::kCharacterOffsets),
            expected_offsets);

  omnibox_textfield()->OnFocus();

  EXPECT_EQ(gfx::NO_ELIDE, render_text->elide_behavior());
  ui::AXNodeData node_data_2;
  omnibox_view()->GetViewAccessibility().GetAccessibleNodeData(&node_data_2);
  std::vector<int32_t> expected_offsets_2 = {
      0,   6,   10,  14,  21,  24,  29,  33,  42,  51,  59,
      62,  68,  74,  80,  90,  97,  100, 107, 109, 115, 122,
      132, 137, 142, 149, 156, 162, 166, 172, 180, 188};
  EXPECT_EQ(node_data_2.GetIntListAttribute(
                ax::mojom::IntListAttribute::kCharacterOffsets),
            expected_offsets_2);
}
#endif  // BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)

TEST_F(OmniboxViewViewsTest, InitialAccessibilityProperties) {
  ui::AXNodeData node_data;
  omnibox_view()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.role, ax::mojom::Role::kTextField);
  EXPECT_EQ(node_data.GetStringAttribute(ax::mojom::StringAttribute::kName),
            "Address and search bar");
  EXPECT_EQ(node_data.GetRestriction(), ax::mojom::Restriction::kNone);
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kEditable));
  EXPECT_TRUE(omnibox_view()->GetViewAccessibility().IsLeaf());
}

TEST_F(OmniboxViewViewsTestIsPopupWindowMode, InitialAccessibilityProperties) {
  ui::AXNodeData node_data;
  omnibox_view()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.role, ax::mojom::Role::kTextField);
  EXPECT_EQ(node_data.GetStringAttribute(ax::mojom::StringAttribute::kName),
            "Address and search bar");
  EXPECT_EQ(node_data.GetRestriction(), ax::mojom::Restriction::kReadOnly);
  EXPECT_TRUE(omnibox_view()->GetViewAccessibility().IsLeaf());
}

TEST_F(OmniboxViewViewsTest, AccessibleValue) {
  // Initial value should be empty.
  ui::AXNodeData node_data;
  omnibox_view()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(std::string(""),
            node_data.GetStringAttribute(ax::mojom::StringAttribute::kValue));

  // Set a value and check that it's reflected in the accessibility tree.
  omnibox_view()->SetWindowTextAndCaretPos(u"google.com", 5, false, false);
  EXPECT_EQ(u"google.com", omnibox_view()->GetText());
  node_data = ui::AXNodeData();
  omnibox_view()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ("google.com",
            node_data.GetStringAttribute(ax::mojom::StringAttribute::kValue));

  // Set a user value and check that it's reflected in the accessibility tree.
  location_bar_model()->set_url(GURL("https://permanent-text.com/"));
  omnibox_view()->model()->ResetDisplayTexts();
  omnibox_view()->RevertAll();

  EXPECT_EQ(u"https://permanent-text.com/", omnibox_view()->GetText());
  EXPECT_FALSE(omnibox_view()->model()->user_input_in_progress());

  omnibox_view()->SetUserText(u"user text");
  EXPECT_EQ(u"user text", omnibox_view()->GetText());
  EXPECT_TRUE(omnibox_view()->model()->user_input_in_progress());
  node_data = ui::AXNodeData();
  omnibox_view()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ("user text",
            node_data.GetStringAttribute(ax::mojom::StringAttribute::kValue));

  // Expect that on Escape, the text is reverted to the permanent URL.
  ui::KeyEvent escape(ui::EventType::kKeyPressed, ui::VKEY_ESCAPE, 0);
  omnibox_textfield()->OnKeyEvent(&escape);

  node_data = ui::AXNodeData();
  omnibox_view()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ("https://permanent-text.com/",
            node_data.GetStringAttribute(ax::mojom::StringAttribute::kValue));
}

class OmniboxViewViewsClipboardTest
    : public OmniboxViewViewsTest,
      public ::testing::WithParamInterface<ui::TextEditCommand> {
 public:
  void SetUp() override {
    OmniboxViewViewsTest::SetUp();

    location_bar_model()->set_url(GURL("https://test.com/"));
    omnibox_view()->model()->ResetDisplayTexts();
    omnibox_view()->RevertAll();
  }
};

TEST_P(OmniboxViewViewsClipboardTest, ClipboardCopyOrCutURL) {
  omnibox_view()->SelectAll(false);
  ASSERT_TRUE(omnibox_view()->IsSelectAll());

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  ui::ClipboardBuffer clipboard_buffer = ui::ClipboardBuffer::kCopyPaste;

  clipboard->Clear(clipboard_buffer);
  ui::TextEditCommand clipboard_command = GetParam();
  GetTextfieldTestApi().ExecuteTextEditCommand(clipboard_command);

  std::u16string expected_text;
  if (clipboard_command == ui::TextEditCommand::COPY)
    expected_text = u"https://test.com/";
  EXPECT_EQ(expected_text, omnibox_view()->GetText());

  // Make sure the plain text format is available, but the HTML one isn't.
  EXPECT_TRUE(clipboard->IsFormatAvailable(
      ui::ClipboardFormatType::PlainTextType(), clipboard_buffer,
      /* data_dst = */ nullptr));
  EXPECT_FALSE(clipboard->IsFormatAvailable(ui::ClipboardFormatType::HtmlType(),
                                            clipboard_buffer,
                                            /* data_dst = */ nullptr));

  // Windows clipboard only supports text URLs.
  // Mac clipboard not reporting URL format available for some reason.
  // crbug.com/751031
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(clipboard->IsFormatAvailable(ui::ClipboardFormatType::UrlType(),
                                           clipboard_buffer,
                                           /* data_dst = */ nullptr));
#endif

  std::string read_from_clipboard;
  clipboard->ReadAsciiText(clipboard_buffer, /* data_dst = */ nullptr,
                           &read_from_clipboard);
  EXPECT_EQ("https://test.com/", read_from_clipboard);
}

TEST_P(OmniboxViewViewsClipboardTest, ClipboardCopyOrCutUserText) {
  omnibox_view()->SetUserText(u"user text");
  omnibox_view()->SelectAll(false);
  ASSERT_TRUE(omnibox_view()->IsSelectAll());

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  ui::ClipboardBuffer clipboard_buffer = ui::ClipboardBuffer::kCopyPaste;

  clipboard->Clear(clipboard_buffer);
  ui::TextEditCommand clipboard_command = GetParam();
  GetTextfieldTestApi().ExecuteTextEditCommand(clipboard_command);

  if (clipboard_command == ui::TextEditCommand::CUT)
    EXPECT_EQ(std::u16string(), omnibox_view()->GetText());

  // Make sure HTML format isn't written. See
  // BookmarkNodeData::WriteToClipboard() for details.
  EXPECT_TRUE(clipboard->IsFormatAvailable(
      ui::ClipboardFormatType::PlainTextType(), clipboard_buffer,
      /* data_dst = */ nullptr));
  EXPECT_FALSE(clipboard->IsFormatAvailable(ui::ClipboardFormatType::HtmlType(),
                                            clipboard_buffer,
                                            /* data_dst = */ nullptr));

  std::string read_from_clipboard;
  clipboard->ReadAsciiText(clipboard_buffer, /* data_dst = */ nullptr,
                           &read_from_clipboard);
  EXPECT_EQ("user text", read_from_clipboard);
}

INSTANTIATE_TEST_SUITE_P(OmniboxViewViewsClipboardTest,
                         OmniboxViewViewsClipboardTest,
                         ::testing::Values(ui::TextEditCommand::COPY,
                                           ui::TextEditCommand::CUT));

class OmniboxViewViewsSteadyStateElisionsTest : public OmniboxViewViewsTest {
 public:
  OmniboxViewViewsSteadyStateElisionsTest() : OmniboxViewViewsTest({}, {}) {}

 protected:
  const int kCharacterWidth = 10;
  const GURL kFullUrl = GURL("https://www.example.com/");

  void SetUp() override {
    OmniboxViewViewsTest::SetUp();

    // Advance 5 seconds from epoch so the time is not considered null.
    clock_.Advance(base::Seconds(5));
    ui::SetEventTickClockForTesting(&clock_);

    location_bar_model()->set_url(kFullUrl);
    location_bar_model()->set_url_for_display(u"example.com");

    gfx::test::RenderTextTestApi render_text_test_api(
        omnibox_view()->GetRenderText());
    render_text_test_api.SetGlyphWidth(kCharacterWidth);

    omnibox_view()->model()->ResetDisplayTexts();
    omnibox_view()->RevertAll();
  }

  void TearDown() override {
    ui::SetEventTickClockForTesting(nullptr);
    OmniboxViewViewsTest::TearDown();
  }

  void BlurOmnibox() {
    ASSERT_TRUE(omnibox_view()->HasFocus());
    omnibox_view()->GetFocusManager()->ClearFocus();
    ASSERT_FALSE(omnibox_view()->HasFocus());
  }

  void ExpectFullUrlDisplayed() {
    EXPECT_EQ(base::UTF8ToUTF16(kFullUrl.spec()), omnibox_view()->GetText());
    EXPECT_FALSE(omnibox_view()->model()->user_input_in_progress());
  }

  bool IsElidedUrlDisplayed() {
    return omnibox_view()->GetText() == u"example.com" &&
           !omnibox_view()->model()->user_input_in_progress();
  }

  // Gets a point at |x_offset| from the beginning of the RenderText.
  gfx::Point GetPointInTextAtXOffset(int x_offset) {
    gfx::Rect bounds = omnibox_view()->GetRenderText()->display_rect();
    return gfx::Point(bounds.x() + x_offset, bounds.y() + bounds.height() / 2);
  }

  // Sends a mouse down and mouse up event at |x_offset| pixels from the
  // beginning of the RenderText.
  void SendMouseClick(int x_offset) {
    gfx::Point point = GetPointInTextAtXOffset(x_offset);
    SendMouseClickAtPoint(point, 1);
  }

  // Sends a mouse down and mouse up event at a point
  // beginning of the RenderText.
  void SendMouseClickAtPoint(gfx::Point point,
                             int click_count,
                             int event_flags = ui::EF_LEFT_MOUSE_BUTTON) {
    auto mouse_pressed =
        CreateMouseEvent(ui::EventType::kMousePressed, point, event_flags);
    mouse_pressed.SetClickCount(click_count);
    omnibox_textfield()->OnMousePressed(mouse_pressed);
    auto mouse_released =
        CreateMouseEvent(ui::EventType::kMouseReleased, point, event_flags);
    mouse_released.SetClickCount(click_count);
    omnibox_textfield()->OnMouseReleased(mouse_released);
  }

  // Used to access members that are marked private in views::TextField.
  base::SimpleTestTickClock* clock() { return &clock_; }

 private:
  base::SimpleTestTickClock clock_;
};

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, UrlStartsInElidedState) {
  EXPECT_TRUE(IsElidedUrlDisplayed());
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, UnelideOnArrowKey) {
  SendMouseClick(0);

  // Right key should unelide and move the cursor to the end.
  omnibox_textfield_view()->OnKeyPressed(
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_RIGHT, 0));
  ExpectFullUrlDisplayed();
  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(23U, start);
  EXPECT_EQ(23U, end);

  // Blur to restore the elided URL, then click on the Omnibox again to refocus.
  BlurOmnibox();
  SendMouseClick(0);

  // Left key should unelide and move the cursor to the beginning of the elided
  // part.
  omnibox_textfield_view()->OnKeyPressed(
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_LEFT, 0));
  ExpectFullUrlDisplayed();
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(12U, start);
  EXPECT_EQ(12U, end);
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, UnelideOnHomeKey) {
  SendMouseClick(0);

  // Home key should unelide and move the cursor to the beginning of the full
  // unelided URL.
  omnibox_textfield_view()->OnKeyPressed(
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_HOME, 0));
  ExpectFullUrlDisplayed();
  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(0U, start);
  EXPECT_EQ(0U, end);
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest,
       UnelideViaEndKeyWorksWithIntranetUrls) {
  location_bar_model()->set_url(GURL("https://foobar/"));
  location_bar_model()->set_formatted_full_url(u"https://foobar");
  location_bar_model()->set_url_for_display(u"foobar/");

  omnibox_view()->model()->ResetDisplayTexts();
  omnibox_view()->RevertAll();

  SendMouseClick(0);

  // End key should unelide and move the cursor to the end of the full URL.
  omnibox_textfield_view()->OnKeyPressed(
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_END, 0));

  EXPECT_EQ(u"https://foobar", omnibox_view()->GetText());
  EXPECT_FALSE(omnibox_view()->model()->user_input_in_progress());

  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(14U, start);
  EXPECT_EQ(14U, end);
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, GestureTaps) {
  ui::GestureEvent tap_down(
      0, 0, 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::EventType::kGestureTapDown));
  omnibox_textfield()->OnGestureEvent(&tap_down);

  // Select all on first tap.
  ui::GestureEventDetails tap_details(ui::EventType::kGestureTap);
  tap_details.set_tap_count(1);
  ui::GestureEvent tap(0, 0, 0, ui::EventTimeForNow(), tap_details);
  omnibox_textfield()->OnGestureEvent(&tap);

  EXPECT_TRUE(omnibox_view()->IsSelectAll());
  EXPECT_TRUE(IsElidedUrlDisplayed());

  // Unelide on second tap (cursor placement).
  omnibox_textfield()->OnGestureEvent(&tap);
  ExpectFullUrlDisplayed();
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, FirstMouseClickFocusesOnly) {
  EXPECT_FALSE(omnibox_view()->IsSelectAll());

  SendMouseClick(0);

  EXPECT_TRUE(IsElidedUrlDisplayed());
  EXPECT_TRUE(omnibox_view()->IsSelectAll());
  EXPECT_TRUE(omnibox_view()->HasFocus());
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, NegligibleDragKeepsElisions) {
  gfx::Point click_point = GetPointInTextAtXOffset(2 * kCharacterWidth);
  omnibox_textfield()->OnMousePressed(
      CreateMouseEvent(ui::EventType::kMousePressed, click_point));

  // Offset the drag and release point by an insignificant 2 px.
  gfx::Point drag_point = click_point;
  drag_point.Offset(2, 0);
  omnibox_textfield()->OnMouseDragged(
      CreateMouseEvent(ui::EventType::kMouseDragged, drag_point));
  omnibox_textfield()->OnMouseReleased(
      CreateMouseEvent(ui::EventType::kMouseReleased, drag_point));

  // Expect that after a negligible drag and release, everything is selected.
  EXPECT_TRUE(IsElidedUrlDisplayed());
  EXPECT_TRUE(omnibox_view()->IsSelectAll());
  EXPECT_TRUE(omnibox_view()->HasFocus());
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, CaretPlacementByMouse) {
  SendMouseClick(0);

  // Advance the clock 5 seconds so the second click is not interpreted as a
  // double click.
  clock()->Advance(base::Seconds(5));

  // Second click should unelide only on mouse release.
  omnibox_textfield()->OnMousePressed(
      CreateMouseEvent(ui::EventType::kMousePressed,
                       GetPointInTextAtXOffset(2 * kCharacterWidth)));
  EXPECT_TRUE(IsElidedUrlDisplayed());
  omnibox_textfield()->OnMouseReleased(
      CreateMouseEvent(ui::EventType::kMouseReleased,
                       GetPointInTextAtXOffset(2 * kCharacterWidth)));
  ExpectFullUrlDisplayed();

  // Verify the cursor position is https://www.ex|ample.com. It should be
  // between 'x' and 'a', because the click was after the second character of
  // the unelided text "example.com".
  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(14U, start);
  EXPECT_EQ(14U, end);
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, MouseDoubleClick) {
  SendMouseClick(4 * kCharacterWidth);

  // Second click without advancing the clock should be a double-click, which
  // should do a single word selection and unelide the text on mousedown.
  omnibox_textfield()->OnMousePressed(
      CreateMouseEvent(ui::EventType::kMousePressed,
                       GetPointInTextAtXOffset(4 * kCharacterWidth)));
  ExpectFullUrlDisplayed();

  // Verify that the selection is https://www.|example|.com, since the
  // double-click after the fourth character of the unelided text "example.com".
  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(12U, start);
  EXPECT_EQ(19U, end);
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, MouseSingleThenDoubleClick) {
  EXPECT_TRUE(IsElidedUrlDisplayed());
  auto point = GetPointInTextAtXOffset(4 * kCharacterWidth);
  SendMouseClickAtPoint(point, 1);
  EXPECT_TRUE(IsElidedUrlDisplayed());
  EXPECT_EQ(u"example.com", omnibox_view()->GetText());

  // Verify that the whole full URL is selected.
  EXPECT_TRUE(omnibox_view()->IsSelectAll());

  // Advance the clock 5 seconds so the next click is not interpreted as a
  // double click.
  clock()->Advance(base::Seconds(5));

  // Double click
  SendMouseClickAtPoint(point, 1);
  ExpectFullUrlDisplayed();
  SendMouseClickAtPoint(point, 2);
  ExpectFullUrlDisplayed();

  // Verify that the selection is https://www.|example|.com, since the
  // double-click after the fourth character of the unelided text "example.com".
  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(12U, start);
  EXPECT_EQ(19U, end);
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, MouseSingleThenRightClick) {
  EXPECT_TRUE(IsElidedUrlDisplayed());
  auto point = GetPointInTextAtXOffset(4 * kCharacterWidth);
  SendMouseClickAtPoint(point, 1);
  EXPECT_TRUE(IsElidedUrlDisplayed());
  EXPECT_EQ(u"example.com", omnibox_view()->GetText());

  // Verify that the whole full URL is selected.
  EXPECT_TRUE(omnibox_view()->IsSelectAll());

  // Advance the clock 5 seconds so the next click is not interpreted as a
  // double click.
  clock()->Advance(base::Seconds(5));

  // Right click
  SendMouseClickAtPoint(point, 1, ui::EF_RIGHT_MOUSE_BUTTON);
  EXPECT_TRUE(IsElidedUrlDisplayed());
  EXPECT_TRUE(omnibox_view()->IsSelectAll());
  EXPECT_TRUE(omnibox_view()->HasFocus());
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, MouseTripleClick) {
  auto point = GetPointInTextAtXOffset(4 * kCharacterWidth);
  SendMouseClickAtPoint(point, 1);
  SendMouseClickAtPoint(point, 2);
  SendMouseClickAtPoint(point, 3);

  ExpectFullUrlDisplayed();

  // Verify that the whole full URL is selected.
  EXPECT_TRUE(omnibox_view()->IsSelectAll());
  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(0U, start);
  EXPECT_EQ(24U, end);
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, MouseClickDrag) {
  omnibox_textfield()->OnMousePressed(
      CreateMouseEvent(ui::EventType::kMousePressed,
                       GetPointInTextAtXOffset(2 * kCharacterWidth)));
  EXPECT_TRUE(IsElidedUrlDisplayed());

  // Expect that during the drag, the URL is still elided.
  omnibox_textfield()->OnMouseDragged(
      CreateMouseEvent(ui::EventType::kMouseDragged,
                       GetPointInTextAtXOffset(4 * kCharacterWidth)));
  EXPECT_TRUE(IsElidedUrlDisplayed());

  // Expect that ex|am|ple.com is the drag selected portion while dragging.
  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(2U, start);
  EXPECT_EQ(4U, end);

  omnibox_textfield()->OnMouseReleased(
      CreateMouseEvent(ui::EventType::kMouseReleased,
                       GetPointInTextAtXOffset(4 * kCharacterWidth)));
  ExpectFullUrlDisplayed();

  // Expect that https://www.ex|am|ple.com is the selected portion after the
  // user releases the mouse.
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(14U, start);
  EXPECT_EQ(16U, end);
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest,
       MouseClickDragToBeginningSelectingText) {
  // Backwards drag-select this portion of the elided URL: |exam|ple.com
  omnibox_textfield()->OnMousePressed(
      CreateMouseEvent(ui::EventType::kMousePressed,
                       GetPointInTextAtXOffset(4 * kCharacterWidth)));
  omnibox_textfield()->OnMouseDragged(
      CreateMouseEvent(ui::EventType::kMouseDragged,
                       GetPointInTextAtXOffset(0 * kCharacterWidth)));
  omnibox_textfield()->OnMouseReleased(
      CreateMouseEvent(ui::EventType::kMouseReleased,
                       GetPointInTextAtXOffset(0 * kCharacterWidth)));
  ExpectFullUrlDisplayed();

  // Since the selection did not look like a URL, expect the following selected
  // selected portion after the user releases the mouse:
  // https://www.|exam|ple.com
  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(16U, start);
  EXPECT_EQ(12U, end);
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest,
       MouseClickDragToBeginningSelectingURL) {
  // Backwards drag-select this portion of the elided URL: |example.co|m
  omnibox_textfield()->OnMousePressed(
      CreateMouseEvent(ui::EventType::kMousePressed,
                       GetPointInTextAtXOffset(10 * kCharacterWidth)));
  omnibox_textfield()->OnMouseDragged(
      CreateMouseEvent(ui::EventType::kMouseDragged,
                       GetPointInTextAtXOffset(0 * kCharacterWidth)));
  omnibox_textfield()->OnMouseReleased(
      CreateMouseEvent(ui::EventType::kMouseReleased,
                       GetPointInTextAtXOffset(0 * kCharacterWidth)));
  ExpectFullUrlDisplayed();

  // Since the selection does look like a URL, expect the following selected
  // selected portion after the user releases the mouse:
  // |https://www.example.co|m
  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(22U, start);
  EXPECT_EQ(0U, end);
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, MouseDoubleClickDrag) {
  // Expect that after a double-click after the third character of the elided
  // text, the text is unelided, and https://www.|example|.com is selected.
  SendMouseClick(4 * kCharacterWidth);
  omnibox_textfield()->OnMousePressed(
      CreateMouseEvent(ui::EventType::kMousePressed,
                       GetPointInTextAtXOffset(4 * kCharacterWidth)));
  ExpectFullUrlDisplayed();
  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(12U, start);
  EXPECT_EQ(19U, end);

  // Expect that negligible drags are ignored immediately after unelision, as
  // the text has likely shifted, and we don't want to accidentally change the
  // selection.
  gfx::Point drag_point = GetPointInTextAtXOffset(4 * kCharacterWidth);
  drag_point.Offset(1, 1);  // Offset test point one pixel in each dimension.
  omnibox_textfield()->OnMouseDragged(
      CreateMouseEvent(ui::EventType::kMouseDragged, drag_point));
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(12U, start);
  EXPECT_EQ(19U, end);

  // Expect that dragging to the fourth character of the full URL (between the
  // the 'p' and the 's' of https), will word-select the scheme, subdomain, and
  // domain, so the new selection will be |https://www.example|.com. The
  // expected selection is backwards, since we are dragging the mouse from the
  // domain to the scheme.
  omnibox_textfield()->OnMouseDragged(
      CreateMouseEvent(ui::EventType::kMouseDragged,
                       GetPointInTextAtXOffset(2 * kCharacterWidth)));
  ExpectFullUrlDisplayed();
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(19U, start);
  EXPECT_EQ(0U, end);

  // Expect the selection to stay the same after mouse-release.
  omnibox_textfield()->OnMouseReleased(
      CreateMouseEvent(ui::EventType::kMouseReleased,
                       GetPointInTextAtXOffset(2 * kCharacterWidth)));
  ExpectFullUrlDisplayed();
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(19U, start);
  EXPECT_EQ(0U, end);
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, ReelideOnBlur) {
  // Double-click should unelide the URL by making a partial selection.
  SendMouseClick(4 * kCharacterWidth);
  SendMouseClick(4 * kCharacterWidth);
  ExpectFullUrlDisplayed();

  BlurOmnibox();
  EXPECT_TRUE(IsElidedUrlDisplayed());
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, DontReelideOnBlurIfEdited) {
  // Double-click should unelide the URL by making a partial selection.
  SendMouseClick(4 * kCharacterWidth);
  SendMouseClick(4 * kCharacterWidth);
  ExpectFullUrlDisplayed();

  // Since the domain word is selected, pressing 'a' should replace the domain.
  ui::KeyEvent char_event(ui::EventType::kKeyPressed, ui::VKEY_A,
                          ui::DomCode::US_A, 0, ui::DomKey::FromCharacter('a'),
                          ui::EventTimeForNow());
  omnibox_textfield()->InsertChar(char_event);
  EXPECT_EQ(u"https://www.a.com/", omnibox_view()->GetText());
  EXPECT_TRUE(omnibox_view()->model()->user_input_in_progress());

  // Now that we've edited the text, blurring should not re-elide the URL.
  BlurOmnibox();
  EXPECT_EQ(u"https://www.a.com/", omnibox_view()->GetText());
  EXPECT_TRUE(omnibox_view()->model()->user_input_in_progress());
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest,
       DontReelideOnBlurIfWidgetDeactivated) {
  SendMouseClick(0);
  SendMouseClick(0);
  ExpectFullUrlDisplayed();

  // Create a different Widget that will take focus away from the test widget
  // containing our test Omnibox.
  std::unique_ptr<views::Widget> other_widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  other_widget->Show();
  ExpectFullUrlDisplayed();

  omnibox_view()->GetWidget()->Activate();
  ExpectFullUrlDisplayed();
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, SaveSelectAllOnBlurAndRefocus) {
  SendMouseClick(0);
  EXPECT_TRUE(IsElidedUrlDisplayed());
  EXPECT_TRUE(omnibox_view()->IsSelectAll());

  // Blurring and refocusing should preserve a select-all state.
  BlurOmnibox();
  omnibox_view()->RequestFocus();
  EXPECT_TRUE(omnibox_view()->HasFocus());
  EXPECT_TRUE(IsElidedUrlDisplayed());
  EXPECT_TRUE(omnibox_view()->IsSelectAll());
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, UnelideFromModel) {
  EXPECT_TRUE(IsElidedUrlDisplayed());

  omnibox_view()->model()->Unelide();
  EXPECT_TRUE(omnibox_view()->IsSelectAll());
  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(24U, start);
  EXPECT_EQ(0U, end);
  ExpectFullUrlDisplayed();
}
