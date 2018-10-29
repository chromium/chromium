// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/command_updater_impl.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_client.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_edit_controller.h"
#include "chrome/browser/ui/omnibox/query_in_omnibox_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/test_toolbar_model.h"
#include "components/omnibox/browser/toolbar_field_trial.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_edit_commands.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/render_text_test_api.h"
#include "ui/views/controls/textfield/textfield_test_api.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager_impl.h"
#endif

using gfx::Range;

namespace {

// TestingOmniboxView ---------------------------------------------------------

class TestingOmniboxView : public OmniboxViewViews {
 public:
  enum BaseTextEmphasis {
    DEEMPHASIZED,
    EMPHASIZED,
    UNSET,
  };

  TestingOmniboxView(OmniboxEditController* controller,
                     std::unique_ptr<OmniboxClient> client);

  static BaseTextEmphasis to_base_text_emphasis(bool emphasize) {
    return emphasize ? EMPHASIZED : DEEMPHASIZED;
  }

  using views::Textfield::GetRenderText;

  void ResetEmphasisTestState();

  void CheckUpdatePopupCallInfo(size_t call_count,
                                const base::string16& text,
                                const Range& selection_range);

  void CheckUpdatePopupNotCalled();

  Range scheme_range() const { return scheme_range_; }
  Range emphasis_range() const { return emphasis_range_; }
  BaseTextEmphasis base_text_emphasis() const { return base_text_emphasis_; }

  // OmniboxViewViews:
  void EmphasizeURLComponents() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {}
  using OmniboxView::IsSelectAll;

 private:
  // OmniboxViewViews:
  // There is no popup and it doesn't actually matter whether we change the
  // visual style of the text, so these methods are all overridden merely to
  // capture relevant state at the time of the call, to be checked by test code.
  void UpdatePopup() override;
  void SetEmphasis(bool emphasize, const Range& range) override;
  void UpdateSchemeStyle(const Range& range) override;

  size_t update_popup_call_count_ = 0;
  base::string16 update_popup_text_;
  Range update_popup_selection_range_;

  // Range of the last scheme logged by UpdateSchemeStyle().
  Range scheme_range_;

  // Range of the last text emphasized by SetEmphasis().
  Range emphasis_range_;

  // SetEmphasis() logs whether the base color of the text is emphasized.
  BaseTextEmphasis base_text_emphasis_ = UNSET;

  DISALLOW_COPY_AND_ASSIGN(TestingOmniboxView);
};

TestingOmniboxView::TestingOmniboxView(OmniboxEditController* controller,
                                       std::unique_ptr<OmniboxClient> client)
    : OmniboxViewViews(controller,
                       std::move(client),
                       false,
                       nullptr,
                       gfx::FontList()) {}

void TestingOmniboxView::ResetEmphasisTestState() {
  base_text_emphasis_ = UNSET;
  emphasis_range_ = Range::InvalidRange();
  scheme_range_ = Range::InvalidRange();
}

void TestingOmniboxView::CheckUpdatePopupCallInfo(
    size_t call_count,
    const base::string16& text,
    const Range& selection_range) {
  EXPECT_EQ(call_count, update_popup_call_count_);
  EXPECT_EQ(text, update_popup_text_);
  EXPECT_EQ(selection_range, update_popup_selection_range_);
}

void TestingOmniboxView::CheckUpdatePopupNotCalled() {
  EXPECT_EQ(update_popup_call_count_, 0U);
}

void TestingOmniboxView::EmphasizeURLComponents() {
  UpdateTextStyle(text(), model()->CurrentTextIsURL(),
                  model()->client()->GetSchemeClassifier());
}

void TestingOmniboxView::UpdatePopup() {
  ++update_popup_call_count_;
  update_popup_text_ = text();
  update_popup_selection_range_ = GetSelectedRange();
}

void TestingOmniboxView::SetEmphasis(bool emphasize, const Range& range) {
  if (range == Range::InvalidRange()) {
    base_text_emphasis_ = to_base_text_emphasis(emphasize);
    return;
  }

  EXPECT_TRUE(emphasize);
  emphasis_range_ = range;
}

void TestingOmniboxView::UpdateSchemeStyle(const Range& range) {
  scheme_range_ = range;
}

// TestingOmniboxEditController -----------------------------------------------

class TestingOmniboxEditController : public ChromeOmniboxEditController {
 public:
  TestingOmniboxEditController(CommandUpdater* command_updater,
                               ToolbarModel* toolbar_model)
      : ChromeOmniboxEditController(command_updater),
        toolbar_model_(toolbar_model) {}

 private:
  // ChromeOmniboxEditController:
  ToolbarModel* GetToolbarModel() override { return toolbar_model_; }
  const ToolbarModel* GetToolbarModel() const override {
    return toolbar_model_;
  }

  ToolbarModel* toolbar_model_;

  DISALLOW_COPY_AND_ASSIGN(TestingOmniboxEditController);
};

}  // namespace

// OmniboxViewViewsTest -------------------------------------------------------

// Base class that ensures ScopedFeatureList is initialized first.
class OmniboxViewViewsTestBase : public ChromeViewsTestBase {
 public:
  explicit OmniboxViewViewsTestBase(
      const std::vector<base::Feature>& enabled_features) {
    scoped_feature_list_.InitWithFeatures(enabled_features, {});
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class OmniboxViewViewsTest : public OmniboxViewViewsTestBase {
 public:
  explicit OmniboxViewViewsTest(
      const std::vector<base::Feature>& enabled_features);

  OmniboxViewViewsTest() : OmniboxViewViewsTest(std::vector<base::Feature>()) {}

  TestToolbarModel* toolbar_model() { return &toolbar_model_; }
  TestingOmniboxView* omnibox_view() const { return omnibox_view_; }
  views::Textfield* omnibox_textfield() const { return omnibox_view(); }
  ui::TextEditCommand scheduled_text_edit_command() const {
    return test_api_->scheduled_text_edit_command();
  }

  // Sets |new_text| as the omnibox text, and emphasizes it appropriately.  If
  // |accept_input| is true, pretends that the user has accepted this input
  // (i.e. it's been navigated to).
  void SetAndEmphasizeText(const std::string& new_text, bool accept_input);

  bool IsCursorEnabled() const {
    return test_api_->GetRenderText()->cursor_enabled();
  }

 protected:
  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  ui::MouseEvent CreateEvent(ui::EventType type, int flags) {
    return ui::MouseEvent(type, gfx::Point(0, 0), gfx::Point(),
                          ui::EventTimeForNow(), flags, 0);
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  TemplateURLServiceFactoryTestUtil util_;
  CommandUpdaterImpl command_updater_;
  TestToolbarModel toolbar_model_;
  TestingOmniboxEditController omnibox_edit_controller_;

  std::unique_ptr<views::Widget> widget_;

  // Owned by |widget_|.
  TestingOmniboxView* omnibox_view_;

  std::unique_ptr<views::TextfieldTestApi> test_api_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxViewViewsTest);
};

OmniboxViewViewsTest::OmniboxViewViewsTest(
    const std::vector<base::Feature>& enabled_features)
    : OmniboxViewViewsTestBase(enabled_features),
      util_(&profile_),
      command_updater_(nullptr),
      omnibox_edit_controller_(&command_updater_, &toolbar_model_) {}

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

  // We need a widget so OmniboxView can be correctly focused and unfocused.
  widget_ = std::make_unique<views::Widget>();
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 400, 80);
  widget_->Init(params);
  widget_->Show();

#if defined(OS_CHROMEOS)
  chromeos::input_method::InitializeForTesting(
      new chromeos::input_method::MockInputMethodManagerImpl);
#endif
  AutocompleteClassifierFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile_,
      base::BindRepeating(&AutocompleteClassifierFactory::BuildInstanceFor));
  QueryInOmniboxFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile_, base::BindRepeating(&QueryInOmniboxFactory::BuildInstanceFor));
  omnibox_view_ = new TestingOmniboxView(
      &omnibox_edit_controller_, std::make_unique<ChromeOmniboxClient>(
                                     &omnibox_edit_controller_, &profile_));
  test_api_ = std::make_unique<views::TextfieldTestApi>(omnibox_view_);
  omnibox_view_->Init();

  widget_->SetContentsView(omnibox_view_);
}

void OmniboxViewViewsTest::TearDown() {
  // Clean ourselves up as the text input client.
  if (omnibox_view_->GetInputMethod())
    omnibox_view_->GetInputMethod()->DetachTextInputClient(omnibox_view_);

  widget_.reset();

#if defined(OS_CHROMEOS)
  chromeos::input_method::Shutdown();
#endif
  ChromeViewsTestBase::TearDown();
}

// Actual tests ---------------------------------------------------------------

// Checks that a single change of the text in the omnibox invokes
// only one call to OmniboxViewViews::UpdatePopup().
TEST_F(OmniboxViewViewsTest, UpdatePopupCall) {
  ui::KeyEvent char_event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::DomCode::US_A, 0,
                          ui::DomKey::FromCharacter('a'),
                          ui::EventTimeForNow());
  omnibox_textfield()->InsertChar(char_event);
  omnibox_view()->CheckUpdatePopupCallInfo(1, base::ASCIIToUTF16("a"),
                                           Range(1));

  char_event =
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_B, ui::DomCode::US_B, 0,
                   ui::DomKey::FromCharacter('b'), ui::EventTimeForNow());
  omnibox_textfield()->InsertChar(char_event);
  omnibox_view()->CheckUpdatePopupCallInfo(2, base::ASCIIToUTF16("ab"),
                                           Range(2));

  ui::KeyEvent pressed(ui::ET_KEY_PRESSED, ui::VKEY_BACK, 0);
  omnibox_textfield()->OnKeyEvent(&pressed);
  omnibox_view()->CheckUpdatePopupCallInfo(3, base::ASCIIToUTF16("a"),
                                           Range(1));
}

// Test that text cursor is shown in the omnibox after entering any single
// character in NTP 'Search box'. Test for crbug.com/698172.
TEST_F(OmniboxViewViewsTest, EditTextfield) {
  omnibox_textfield()->SetCursorEnabled(false);
  ui::KeyEvent char_event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::DomCode::US_A, 0,
                          ui::DomKey::FromCharacter('a'),
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
  EXPECT_EQ(ui::TextEditCommand::MOVE_UP, scheduled_text_edit_command());

  ui::KeyEvent up_pressed(ui::ET_KEY_PRESSED, ui::VKEY_UP, 0);
  omnibox_textfield()->OnKeyEvent(&up_pressed);
  EXPECT_EQ(ui::TextEditCommand::INVALID_COMMAND,
            scheduled_text_edit_command());
}

// Test that Shift+Up and Shift+Down are not captured and let selection mode
// take over. Test for crbug.com/863543 and crbug.com/892216.
TEST_F(OmniboxViewViewsTest, SelectWithShift_863543) {
  const base::string16 text =
      base::ASCIIToUTF16("http://www.example.com/?query=1");
  static_cast<OmniboxView*>(omnibox_view())
      ->SetWindowTextAndCaretPos(text, 23U, false, false);

  ui::KeyEvent shift_up_pressed(ui::ET_KEY_PRESSED, ui::VKEY_UP,
                                ui::EF_SHIFT_DOWN);
  omnibox_textfield()->OnKeyEvent(&shift_up_pressed);

  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(23U, start);
  EXPECT_EQ(0U, end);
  omnibox_view()->CheckUpdatePopupNotCalled();

  static_cast<OmniboxView*>(omnibox_view())
      ->SetWindowTextAndCaretPos(text, 18U, false, false);

  ui::KeyEvent shift_down_pressed(ui::ET_KEY_PRESSED, ui::VKEY_DOWN,
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
  omnibox_view()->OnFocus();
  const base::string16 kContentsRtl =
      base::WideToUTF16(L"\x05e8\x05e2.\x05e7\x05d5\x05dd/0123/abcd");
  static_cast<OmniboxView*>(omnibox_view())
      ->SetWindowTextAndCaretPos(kContentsRtl, 0, false, false);
  EXPECT_EQ(gfx::NO_ELIDE, render_text->elide_behavior());
  // NOTE: Technically (depending on the font), this expectation could fail if
  // the entire domain fits in 60 pixels. However, 60px is so small it should
  // never happen with any font.
  EXPECT_GT(0, render_text->GetUpdatedDisplayOffset().x());
  omnibox_view()->SelectAll(false);
  EXPECT_TRUE(omnibox_view()->IsSelectAll());

  // Now enter blurred mode, where the text should be elided to 60px. This means
  // the string itself is truncated. Scrolling would therefore mean the text is
  // off-screen. Ensure that the horizontal scrolling has been reset to 0.
  omnibox_view()->OnBlur();
  EXPECT_EQ(gfx::ELIDE_TAIL, render_text->elide_behavior());
  EXPECT_EQ(0, render_text->GetUpdatedDisplayOffset().x());
  EXPECT_FALSE(omnibox_view()->IsSelectAll());
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
    EXPECT_EQ(TestingOmniboxView::to_base_text_emphasis(
                  test_case.expected_base_text_emphasized),
              omnibox_view()->base_text_emphasis());
    EXPECT_EQ(test_case.expected_emphasis_range,
              omnibox_view()->emphasis_range());
    EXPECT_FALSE(omnibox_view()->scheme_range().IsValid());

    if (test_case.expected_scheme_range.IsValid()) {
      SetAndEmphasizeText(test_case.input, true);
      EXPECT_EQ(TestingOmniboxView::to_base_text_emphasis(
                    test_case.expected_base_text_emphasized),
                omnibox_view()->base_text_emphasis());
      EXPECT_EQ(test_case.expected_emphasis_range,
                omnibox_view()->emphasis_range());
      EXPECT_EQ(test_case.expected_scheme_range,
                omnibox_view()->scheme_range());
    }
  }
}

TEST_F(OmniboxViewViewsTest, RevertOnBlur) {
  toolbar_model()->set_url(GURL("https://permanent-text.com/"));
  omnibox_view()->model()->ResetDisplayTexts();
  omnibox_view()->RevertAll();

  EXPECT_EQ(base::ASCIIToUTF16("https://permanent-text.com/"),
            omnibox_view()->text());
  EXPECT_FALSE(omnibox_view()->model()->user_input_in_progress());

  omnibox_view()->SetUserText(base::ASCIIToUTF16("user text"));

  EXPECT_EQ(base::ASCIIToUTF16("user text"), omnibox_view()->text());
  EXPECT_TRUE(omnibox_view()->model()->user_input_in_progress());

  // Expect that on blur, if the text has been edited, stay in user input mode.
  omnibox_textfield()->OnBlur();
  EXPECT_EQ(base::ASCIIToUTF16("user text"), omnibox_view()->text());
  EXPECT_TRUE(omnibox_view()->model()->user_input_in_progress());

  // Expect that on blur, if the text is the same as the
  // https://permanent-text.com, exit user input mode.
  omnibox_view()->SetUserText(
      base::ASCIIToUTF16("https://permanent-text.com/"));
  EXPECT_TRUE(omnibox_view()->model()->user_input_in_progress());
  omnibox_textfield()->OnBlur();
  EXPECT_EQ(base::ASCIIToUTF16("https://permanent-text.com/"),
            omnibox_view()->text());
  EXPECT_FALSE(omnibox_view()->model()->user_input_in_progress());
}

TEST_F(OmniboxViewViewsTest, BackspaceExitsKeywordMode) {
  omnibox_view()->SetUserText(base::UTF8ToUTF16("user text"));
  omnibox_view()->model()->EnterKeywordModeForDefaultSearchProvider(
      KeywordModeEntryMethod::KEYBOARD_SHORTCUT);

  ASSERT_EQ(base::UTF8ToUTF16("user text"), omnibox_view()->GetText());
  ASSERT_TRUE(omnibox_view()->IsSelectAll());
  ASSERT_FALSE(omnibox_view()->model()->keyword().empty());

  // First backspace should clear the user text but not exit keyword mode.
  ui::KeyEvent backspace(ui::ET_KEY_PRESSED, ui::VKEY_BACK, 0);
  omnibox_textfield()->OnKeyEvent(&backspace);
  EXPECT_TRUE(omnibox_view()->GetText().empty());
  EXPECT_FALSE(omnibox_view()->model()->keyword().empty());

  // Second backspace should exit keyword mode.
  omnibox_textfield()->OnKeyEvent(&backspace);
  EXPECT_TRUE(omnibox_view()->GetText().empty());
  EXPECT_TRUE(omnibox_view()->model()->keyword().empty());
}

class OmniboxViewViewsSteadyStateElisionsTest : public OmniboxViewViewsTest {
 public:
  OmniboxViewViewsSteadyStateElisionsTest()
      : OmniboxViewViewsTest({
            toolbar::features::kHideSteadyStateUrlScheme,
            toolbar::features::kHideSteadyStateUrlTrivialSubdomains,
        }) {}

 protected:
  explicit OmniboxViewViewsSteadyStateElisionsTest(
      const std::vector<base::Feature>& enabled_features)
      : OmniboxViewViewsTest(enabled_features) {}

  const int kCharacterWidth = 10;
  const GURL kFullUrl = GURL("https://www.example.com/");

  void SetUp() override {
    OmniboxViewViewsTest::SetUp();

    // Advance 5 seconds from epoch so the time is not considered null.
    clock_.Advance(base::TimeDelta::FromSeconds(5));
    ui::SetEventTickClockForTesting(&clock_);

    toolbar_model()->set_url(kFullUrl);
    toolbar_model()->set_url_for_display(base::ASCIIToUTF16("example.com"));

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
    EXPECT_EQ(base::UTF8ToUTF16(kFullUrl.spec()), omnibox_view()->text());
    EXPECT_TRUE(omnibox_view()->model()->user_input_in_progress());

    // We test the user text stored in the model has been updated as well. The
    // model user text is used to populate the text in the Omnibox after some
    // state transitions, such as the ZeroSuggest popup opening.
    EXPECT_EQ(base::UTF8ToUTF16(kFullUrl.spec()),
              omnibox_view()->model()->GetUserTextForTesting());
  }

  bool IsElidedUrlDisplayed() {
    return omnibox_view()->text() == base::ASCIIToUTF16("example.com") &&
           !omnibox_view()->model()->user_input_in_progress();
  }

  ui::MouseEvent CreateMouseEvent(ui::EventType type, const gfx::Point& point) {
    return ui::MouseEvent(type, point, gfx::Point(), ui::EventTimeForNow(),
                          ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
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
    omnibox_view()->OnMousePressed(
        CreateMouseEvent(ui::ET_MOUSE_PRESSED, point));
    omnibox_view()->OnMouseReleased(
        CreateMouseEvent(ui::ET_MOUSE_RELEASED, point));
  }

  // Used to access members that are marked private in views::TextField.
  views::View* omnibox_textfield_view() { return omnibox_view(); }
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
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RIGHT, 0));
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
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_LEFT, 0));
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
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_HOME, 0));
  ExpectFullUrlDisplayed();
  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(0U, start);
  EXPECT_EQ(0U, end);
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, GestureTaps) {
  ui::GestureEvent tap_down(0, 0, 0, ui::EventTimeForNow(),
                            ui::GestureEventDetails(ui::ET_GESTURE_TAP_DOWN));
  omnibox_textfield_view()->OnGestureEvent(&tap_down);

  // Select all on first tap.
  ui::GestureEventDetails tap_details(ui::ET_GESTURE_TAP);
  tap_details.set_tap_count(1);
  ui::GestureEvent tap(0, 0, 0, ui::EventTimeForNow(), tap_details);
  omnibox_textfield_view()->OnGestureEvent(&tap);

  EXPECT_TRUE(omnibox_view()->IsSelectAll());
  EXPECT_TRUE(IsElidedUrlDisplayed());

  // Unelide on second tap (cursor placement).
  omnibox_textfield_view()->OnGestureEvent(&tap);
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
  omnibox_view()->OnMousePressed(
      CreateMouseEvent(ui::ET_MOUSE_PRESSED, click_point));

  // Offset the drag and release point by an insignificant 2 px.
  gfx::Point drag_point = click_point;
  drag_point.Offset(2, 0);
  omnibox_view()->OnMouseDragged(
      CreateMouseEvent(ui::ET_MOUSE_DRAGGED, drag_point));
  omnibox_view()->OnMouseReleased(
      CreateMouseEvent(ui::ET_MOUSE_RELEASED, drag_point));

  // Expect that after a negligible drag and release, everything is selected.
  EXPECT_TRUE(IsElidedUrlDisplayed());
  EXPECT_TRUE(omnibox_view()->IsSelectAll());
  EXPECT_TRUE(omnibox_view()->HasFocus());
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, CaretPlacementByMouse) {
  SendMouseClick(0);

  // Advance the clock 5 seconds so the second click is not interpreted as a
  // double click.
  clock()->Advance(base::TimeDelta::FromSeconds(5));

  // Second click should unelide only on mouse release.
  omnibox_view()->OnMousePressed(CreateMouseEvent(
      ui::ET_MOUSE_PRESSED, GetPointInTextAtXOffset(2 * kCharacterWidth)));
  EXPECT_TRUE(IsElidedUrlDisplayed());
  omnibox_view()->OnMouseReleased(CreateMouseEvent(
      ui::ET_MOUSE_RELEASED, GetPointInTextAtXOffset(2 * kCharacterWidth)));
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
  omnibox_view()->OnMousePressed(CreateMouseEvent(
      ui::ET_MOUSE_PRESSED, GetPointInTextAtXOffset(4 * kCharacterWidth)));
  ExpectFullUrlDisplayed();

  // Verify that the selection is https://www.|example|.com, since the
  // double-click after the fourth character of the unelided text "example.com".
  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(12U, start);
  EXPECT_EQ(19U, end);
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, MouseTripleClick) {
  SendMouseClick(4 * kCharacterWidth);
  SendMouseClick(4 * kCharacterWidth);
  SendMouseClick(4 * kCharacterWidth);

  ExpectFullUrlDisplayed();

  // Verify that the whole full URL is selected.
  EXPECT_TRUE(omnibox_view()->IsSelectAll());
  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(0U, start);
  EXPECT_EQ(24U, end);
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, MouseClickDrag) {
  omnibox_view()->OnMousePressed(CreateMouseEvent(
      ui::ET_MOUSE_PRESSED, GetPointInTextAtXOffset(2 * kCharacterWidth)));
  EXPECT_TRUE(IsElidedUrlDisplayed());

  // Expect that during the drag, the URL is still elided.
  omnibox_view()->OnMouseDragged(CreateMouseEvent(
      ui::ET_MOUSE_DRAGGED, GetPointInTextAtXOffset(4 * kCharacterWidth)));
  EXPECT_TRUE(IsElidedUrlDisplayed());

  // Expect that ex|am|ple.com is the drag selected portion while dragging.
  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(2U, start);
  EXPECT_EQ(4U, end);

  omnibox_view()->OnMouseReleased(CreateMouseEvent(
      ui::ET_MOUSE_RELEASED, GetPointInTextAtXOffset(4 * kCharacterWidth)));
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
  omnibox_view()->OnMousePressed(CreateMouseEvent(
      ui::ET_MOUSE_PRESSED, GetPointInTextAtXOffset(4 * kCharacterWidth)));
  omnibox_view()->OnMouseDragged(CreateMouseEvent(
      ui::ET_MOUSE_DRAGGED, GetPointInTextAtXOffset(0 * kCharacterWidth)));
  omnibox_view()->OnMouseReleased(CreateMouseEvent(
      ui::ET_MOUSE_RELEASED, GetPointInTextAtXOffset(0 * kCharacterWidth)));
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
  omnibox_view()->OnMousePressed(CreateMouseEvent(
      ui::ET_MOUSE_PRESSED, GetPointInTextAtXOffset(10 * kCharacterWidth)));
  omnibox_view()->OnMouseDragged(CreateMouseEvent(
      ui::ET_MOUSE_DRAGGED, GetPointInTextAtXOffset(0 * kCharacterWidth)));
  omnibox_view()->OnMouseReleased(CreateMouseEvent(
      ui::ET_MOUSE_RELEASED, GetPointInTextAtXOffset(0 * kCharacterWidth)));
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
  omnibox_view()->OnMousePressed(CreateMouseEvent(
      ui::ET_MOUSE_PRESSED, GetPointInTextAtXOffset(4 * kCharacterWidth)));
  ExpectFullUrlDisplayed();
  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(12U, start);
  EXPECT_EQ(19U, end);

  // Expect that dragging to the fourth character of the full URL (between the
  // the 'p' and the 's' of https), will word-select the scheme, subdomain, and
  // domain, so the new selection will be |https://www.example|.com. The
  // expected selection is backwards, since we are dragging the mouse from the
  // domain to the scheme.
  omnibox_view()->OnMouseDragged(CreateMouseEvent(
      ui::ET_MOUSE_DRAGGED, GetPointInTextAtXOffset(2 * kCharacterWidth)));
  ExpectFullUrlDisplayed();
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(19U, start);
  EXPECT_EQ(0U, end);

  // Expect the selection to stay the same after mouse-release.
  omnibox_view()->OnMouseReleased(CreateMouseEvent(
      ui::ET_MOUSE_RELEASED, GetPointInTextAtXOffset(2 * kCharacterWidth)));
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
  ui::KeyEvent char_event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::DomCode::US_A, 0,
                          ui::DomKey::FromCharacter('a'),
                          ui::EventTimeForNow());
  omnibox_textfield()->InsertChar(char_event);
  EXPECT_EQ(base::ASCIIToUTF16("https://www.a.com/"), omnibox_view()->text());
  EXPECT_TRUE(omnibox_view()->model()->user_input_in_progress());

  // Now that we've edited the text, blurring should not re-elide the URL.
  BlurOmnibox();
  EXPECT_EQ(base::ASCIIToUTF16("https://www.a.com/"), omnibox_view()->text());
  EXPECT_TRUE(omnibox_view()->model()->user_input_in_progress());
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest,
       DontReelideOnBlurIfWidgetDeactivated) {
  SendMouseClick(0);
  SendMouseClick(0);
  ExpectFullUrlDisplayed();

  // Create a different Widget that will take focus away from the test widget
  // containing our test Omnibox.
  auto other_widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 100, 100);
  other_widget->Init(params);
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

  omnibox_view()->model()->Unelide(false /* exit_query_in_omnibox */);
  EXPECT_TRUE(omnibox_view()->IsSelectAll());
  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(24U, start);
  EXPECT_EQ(0U, end);
  ExpectFullUrlDisplayed();
}

class OmniboxViewViewsSteadyStateElisionsAndQueryInOmniboxTest
    : public OmniboxViewViewsSteadyStateElisionsTest {
 public:
  OmniboxViewViewsSteadyStateElisionsAndQueryInOmniboxTest()
      : OmniboxViewViewsSteadyStateElisionsTest({
            toolbar::features::kHideSteadyStateUrlScheme,
            toolbar::features::kHideSteadyStateUrlTrivialSubdomains,
            omnibox::kQueryInOmnibox,
        }) {}

 protected:
  const GURL kValidSearchResultsPage =
      GURL("https://www.google.com/search?q=foo+query");

  void SetUp() override {
    OmniboxViewViewsSteadyStateElisionsTest::SetUp();

    toolbar_model()->set_url(kValidSearchResultsPage);
    toolbar_model()->set_security_level(security_state::SecurityLevel::SECURE);

    omnibox_view()->model()->ResetDisplayTexts();
    omnibox_view()->RevertAll();

    // Sanity check that Query in Omnibox is working with Steady State Elisions.
    EXPECT_EQ(base::ASCIIToUTF16("foo query"), omnibox_view()->text());

    // Focus the Omnibox.
    SendMouseClick(0);
  }
};

TEST_F(OmniboxViewViewsSteadyStateElisionsAndQueryInOmniboxTest,
       DontUnelideQueryInOmniboxSearchTerms) {
  // Right key should NOT unelide, and should correctly place the cursor at the
  // end of the search query.
  omnibox_textfield_view()->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RIGHT, 0));
  EXPECT_EQ(base::ASCIIToUTF16("foo query"), omnibox_view()->text());
  EXPECT_FALSE(omnibox_view()->model()->user_input_in_progress());

  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(9U, start);
  EXPECT_EQ(9U, end);
}

TEST_F(OmniboxViewViewsSteadyStateElisionsAndQueryInOmniboxTest,
       UnelideFromModel) {
  // Uneliding without exiting Query in Omnibox should do nothing.
  omnibox_view()->model()->Unelide(false /* exit_query_in_omnibox */);
  EXPECT_EQ(base::ASCIIToUTF16("foo query"), omnibox_view()->text());
  {
    size_t start, end;
    omnibox_view()->GetSelectionBounds(&start, &end);
    EXPECT_EQ(9U, start);
    EXPECT_EQ(0U, end);
  }

  // Uneliding and exiting Query in Omnibox should reveal the full URL.
  omnibox_view()->model()->Unelide(true /* exit_query_in_omnibox */);
  EXPECT_EQ(base::ASCIIToUTF16(kValidSearchResultsPage.spec()),
            omnibox_view()->text());
  {
    size_t start, end;
    omnibox_view()->GetSelectionBounds(&start, &end);
    EXPECT_EQ(41U, start);
    EXPECT_EQ(0U, end);
  }
}
