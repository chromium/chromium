// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/controls/hover_button.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/style/typography.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_utils.h"

namespace {

constexpr int kButtonWidth = 150;

struct TitleSubtitlePair {
  const char* const title;
  const char* const subtitle;
  // Whether the HoverButton is expected to have a tooltip for this text.
  bool tooltip;
};

constexpr TitleSubtitlePair kTitleSubtitlePairs[] = {
    // Two short strings that will fit in the space given.
    {"Clap!", "Clap!", false},
    // First string fits, second string doesn't.
    {"If you're happy and you know it, clap your hands!", "Clap clap!", true},
    // Second string fits, first string doesn't.
    {"Clap clap!",
     "If you're happy and you know it, and you really want to show it,", true},
    // Both strings don't fit.
    {"If you're happy and you know it, and you really want to show it,",
     "If you're happy and you know it, clap your hands!", true},
};

class HoverButtonTest : public ChromeViewsTestBase {
 public:
  HoverButtonTest() {}

  HoverButtonTest(const HoverButtonTest&) = delete;
  HoverButtonTest& operator=(const HoverButtonTest&) = delete;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget();
    generator_ = std::make_unique<ui::test::EventGenerator>(
        GetRootWindow(widget_.get()), widget_->GetNativeWindow());
  }

  void TearDown() override {
    widget_.reset();
    generator_.reset();
    ChromeViewsTestBase::TearDown();
  }

  std::unique_ptr<views::View> CreateIcon() {
    auto icon = std::make_unique<views::View>();
    icon->SetPreferredSize(gfx::Size(16, 16));
    return icon;
  }

  ui::test::EventGenerator* generator() { return generator_.get(); }
  views::Widget* widget() { return widget_.get(); }

 private:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
};

// Double check the length of the strings used for testing are either over or
// under the width used for the following tests.
TEST_F(HoverButtonTest, ValidateTestData) {
  auto get_width = [](const char* text) {
    return views::Label(base::ASCIIToUTF16(text)).GetPreferredSize().width();
  };
  EXPECT_GT(kButtonWidth, get_width(kTitleSubtitlePairs[0].title));
  EXPECT_GT(kButtonWidth, get_width(kTitleSubtitlePairs[0].subtitle));

  EXPECT_LT(kButtonWidth, get_width(kTitleSubtitlePairs[1].title));
  EXPECT_GT(kButtonWidth, get_width(kTitleSubtitlePairs[1].subtitle));

  EXPECT_GT(kButtonWidth, get_width(kTitleSubtitlePairs[2].title));
  EXPECT_LT(kButtonWidth, get_width(kTitleSubtitlePairs[2].subtitle));

  EXPECT_LT(kButtonWidth, get_width(kTitleSubtitlePairs[3].title));
  EXPECT_LT(kButtonWidth, get_width(kTitleSubtitlePairs[3].subtitle));
}

// Tests whether the HoverButton has the correct tooltip and accessible name.
TEST_F(HoverButtonTest, TooltipAndAccessibleName) {
  for (size_t i = 0; i < std::size(kTitleSubtitlePairs); ++i) {
    TitleSubtitlePair pair = kTitleSubtitlePairs[i];
    SCOPED_TRACE(testing::Message() << "Index: " << i << ", expected_tooltip="
                                    << (pair.tooltip ? "true" : "false"));
    auto button = std::make_unique<HoverButton>(
        views::Button::PressedCallback(), CreateIcon(),
        base::ASCIIToUTF16(pair.title), base::ASCIIToUTF16(pair.subtitle));
    button->SetSize(gfx::Size(kButtonWidth, 40));

    ui::AXNodeData data;
    button->GetAccessibleNodeData(&data);
    std::string accessible_name;
    data.GetStringAttribute(ax::mojom::StringAttribute::kName,
                            &accessible_name);

    // The accessible name should always be the title and subtitle concatenated
    // by \n.
    std::u16string expected = base::JoinString(
        {base::ASCIIToUTF16(pair.title), base::ASCIIToUTF16(pair.subtitle)},
        u"\n");
    EXPECT_EQ(expected, base::UTF8ToUTF16(accessible_name));

    EXPECT_EQ(pair.tooltip ? expected : std::u16string(),
              button->GetTooltipText(gfx::Point()));
  }
}

// Tests that a button with a subtitle and icons can be instantiated without a
// crash.
TEST_F(HoverButtonTest, CreateButtonWithSubtitleAndIcons) {
  std::unique_ptr<views::View> primary_icon = CreateIcon();
  views::View* primary_icon_raw = primary_icon.get();
  std::unique_ptr<views::View> secondary_icon = CreateIcon();
  views::View* secondary_icon_raw = secondary_icon.get();

  HoverButton button(views::Button::PressedCallback(), std::move(primary_icon),
                     u"Title", u"Subtitle", std::move(secondary_icon));
  EXPECT_TRUE(button.Contains(primary_icon_raw));
  EXPECT_TRUE(button.Contains(secondary_icon_raw));
}

// Tests that the button is activated on mouse release rather than mouse press.
TEST_F(HoverButtonTest, ActivatesOnMouseReleased) {
  bool clicked = false;
  HoverButton* button = widget()->SetContentsView(std::make_unique<HoverButton>(
      base::BindRepeating([](bool* clicked) { *clicked = true; }, &clicked),
      CreateIcon(), u"Title", std::u16string()));
  button->SetBoundsRect(gfx::Rect(100, 100, 200, 200));
  widget()->Show();

  // Button callback should not be called on press.
  generator()->PressLeftButton();
  EXPECT_FALSE(clicked);

  // Button callback should be called on release.
  generator()->ReleaseLeftButton();
  EXPECT_TRUE(clicked);

  widget()->Close();
}

// Test that changing the text style updates the return value of
// views::View::GetHeightForWidth().
TEST_F(HoverButtonTest, ChangingTextStyleResizesButton) {
  auto button = std::make_unique<HoverButton>(
      views::Button::PressedCallback(), CreateIcon(), u"Title", u"Subtitle");
  button->SetSubtitleTextStyle(views::style::CONTEXT_LABEL,
                               views::style::STYLE_SECONDARY);
  int height1 = button->GetHeightForWidth(100);
  button->SetSubtitleTextStyle(views::style::CONTEXT_DIALOG_TITLE,
                               views::style::STYLE_SECONDARY);
  int height2 = button->GetHeightForWidth(100);
  EXPECT_NE(height1, height2);
}

// No touch on desktop Mac.
#if !BUILDFLAG(IS_MAC) || defined(USE_AURA)

// Tests that tapping hover button does not crash if the tap handler removes the
// button from views hierarchy.
TEST_F(HoverButtonTest, TapGestureThatDeletesTheButton) {
  bool clicked = false;
  HoverButton* button = widget()->SetContentsView(std::make_unique<HoverButton>(
      base::BindRepeating(
          [](bool* clicked, views::Widget* widget) {
            *clicked = true;
            // Update the widget contents view, which deletes the hover button.
            widget->SetContentsView(std::make_unique<views::View>());
          },
          &clicked, widget()),
      CreateIcon(), u"Title", std::u16string()));
  button->SetBoundsRect(gfx::Rect(100, 100, 200, 200));
  widget()->Show();

  generator()->GestureTapAt(gfx::Point(150, 150));
  EXPECT_TRUE(clicked);

  widget()->Close();
}

#endif  // !BUILDFLAG(IS_MAC) || defined(USE_AURA)

}  // namespace
