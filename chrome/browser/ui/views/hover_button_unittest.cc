// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/views/controls/hover_button.h"

#include <memory>
#include <string>

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/style/typography.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget_utils.h"

namespace {

constexpr int kButtonWidth = 150;

struct TitleSubtitlePair {
  const std::u16string title;
  const std::u16string subtitle;
  // Whether the HoverButton is expected to have a tooltip for this text.
  bool tooltip;
};

const TitleSubtitlePair kTitleSubtitlePairs[] = {
    // Two short strings that will fit in the space given.
    {u"Clap!", u"Clap!", false},
    // First string fits, second string doesn't.
    {u"If you're happy and you know it, clap your hands!", u"Clap clap!", true},
    // Second string fits, first string doesn't.
    {u"Clap clap!",
     u"If you're happy and you know it, and you really want to show it,", true},
    // Both strings don't fit.
    {u"If you're happy and you know it, and you really want to show it,",
     u"If you're happy and you know it, clap your hands!", true},
};

// Returns the accessible name of `button`.
std::u16string GetAccessibleName(HoverButton& button) {
  ui::AXNodeData data;
  button.GetViewAccessibility().GetAccessibleNodeData(&data);
  return data.GetString16Attribute(ax::mojom::StringAttribute::kName);
}

}  // namespace

class HoverButtonTest : public ChromeViewsTestBase {
 public:
  HoverButtonTest() {}

  HoverButtonTest(const HoverButtonTest&) = delete;
  HoverButtonTest& operator=(const HoverButtonTest&) = delete;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
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

  views::Label* GetButtonSubtitle(const HoverButton& button) {
    return button.subtitle();
  }

  views::Label* GetButtonFooter(const HoverButton& button) {
    return button.footer();
  }

  views::View* GetButtonIconWrapper(const HoverButton& button) {
    return button.icon_wrapper_;
  }

 private:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
};

// Tests whether the HoverButton has the correct tooltip and accessible name.
TEST_F(HoverButtonTest, TooltipAndAccessibleName) {
  for (size_t i = 0; i < std::size(kTitleSubtitlePairs); ++i) {
    TitleSubtitlePair pair = kTitleSubtitlePairs[i];
    SCOPED_TRACE(testing::Message() << "Index: " << i << ", expected_tooltip="
                                    << (pair.tooltip ? "true" : "false"));
    auto button =
        std::make_unique<HoverButton>(views::Button::PressedCallback(),
                                      CreateIcon(), pair.title, pair.subtitle);
    views::IgnoreMissingWidgetForTestingScopedSetter ignore_missing_widget(
        button->GetViewAccessibility());

    button->SetSize(gfx::Size(kButtonWidth, 40));

    // The accessible name should always be the title and subtitle concatenated
    // by \n.
    const std::u16string expected =
        base::StrCat({pair.title, u"\n", pair.subtitle});
    EXPECT_EQ(expected, GetAccessibleName(*button));

    EXPECT_EQ(pair.tooltip ? expected : std::u16string(),
              button->GetTooltipText(gfx::Point()));
  }
}

TEST_F(HoverButtonTest, TooltipAndAccessibleNameWithFooter) {
  auto button = std::make_unique<HoverButton>(
      views::Button::PressedCallback(), CreateIcon(), u"Title", u"Subtitle",
      /*secondary_icon=*/nullptr,
      /*add_vertical_label_spacing=*/true, u"Footer");
  button->SetSize(gfx::Size(kButtonWidth, 40));
  // The accessible name should be the title, subtitle, and footer concatenated
  // by \n.
  const std::u16string expected = u"Title\nSubtitle\nFooter";

  views::IgnoreMissingWidgetForTestingScopedSetter ignore_missing_widget(
      button->GetViewAccessibility());

  EXPECT_EQ(expected, GetAccessibleName(*button));
  EXPECT_EQ(std::u16string(), button->GetTooltipText(gfx::Point()));
}

TEST_F(HoverButtonTest, TooltipAndAccessibleName_DynamicTextUpdate) {
  std::u16string original_title = u"Title";
  std::u16string original_subtitle = u"Subtitle";

  auto button = std::make_unique<HoverButton>(views::Button::PressedCallback(),
                                              CreateIcon(), original_title,
                                              original_subtitle);
  button->SetSize(gfx::Size(kButtonWidth, 40));

  views::IgnoreMissingWidgetForTestingScopedSetter ignore_missing_widget(
      button->GetViewAccessibility());

  // Verify accessible has the original title and subtitle text, and tooltip is
  // empty since text fits in the button.
  std::u16string expected =
      base::StrCat({original_title, u"\n", original_subtitle});
  EXPECT_EQ(expected, GetAccessibleName(*button));
  EXPECT_EQ(std::u16string(), button->GetTooltipText());

  // Update the title with text that still fits in the button.
  std::u16string updated_title = u"New title";
  button->title()->SetText(updated_title);

  // Verify accessible name has the updated title, and tooltip is still empty
  // since text fits in the button.
  expected = base::StrCat({updated_title, u"\n", original_subtitle});
  EXPECT_EQ(expected, GetAccessibleName(*button));
  EXPECT_EQ(std::u16string(), button->GetTooltipText());

  // Update the subtitle with text that doesn't fit in the button.
  std::u16string updated_subtitle =
      u"A very long new subtitle that should not fit in the button";
  GetButtonSubtitle(*button)->SetText(updated_subtitle);

  // Verify both accessible name and tooltip have the updated title and
  // subtitle.
  expected = base::StrCat({updated_title, u"\n", updated_subtitle});
  EXPECT_EQ(expected, GetAccessibleName(*button));
  EXPECT_EQ(expected, button->GetTooltipText());
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

// Tests a button with a subtitle and a footer.
TEST_F(HoverButtonTest, CreateButtonWithSubtitleAndFooter) {
  std::unique_ptr<views::View> primary_icon = CreateIcon();
  views::View* primary_icon_raw = primary_icon.get();
  std::unique_ptr<views::View> secondary_icon = CreateIcon();
  views::View* secondary_icon_raw = secondary_icon.get();
  HoverButton button(views::Button::PressedCallback(), std::move(primary_icon),
                     u"Title", u"Subtitle", std::move(secondary_icon),
                     /*add_vertical_label_spacing=*/true, u"Footer");
  EXPECT_TRUE(button.Contains(primary_icon_raw));
  EXPECT_TRUE(button.Contains(secondary_icon_raw));
  EXPECT_EQ(button.title()->GetText(), u"Title");
  EXPECT_EQ(GetButtonSubtitle(button)->GetText(), u"Subtitle");
  EXPECT_EQ(GetButtonFooter(button)->GetText(), u"Footer");
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

TEST_F(HoverButtonTest, SetIconHorizontalMargins) {
  std::unique_ptr<views::View> primary_icon = CreateIcon();

  HoverButton button(views::Button::PressedCallback(), std::move(primary_icon),
                     u"Title");
  button.SetIconHorizontalMargins(/*left=*/3, /*right=*/4);
  gfx::Insets* margins =
      GetButtonIconWrapper(button)->GetProperty(views::kMarginsKey);
  EXPECT_EQ(margins->left(), 3);
  EXPECT_EQ(margins->right(), 4);
}

#endif  // !BUILDFLAG(IS_MAC) || defined(USE_AURA)
