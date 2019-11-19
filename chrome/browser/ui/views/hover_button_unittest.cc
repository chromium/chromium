// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/hover_button.h"

#include <memory>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
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

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    CreateWidget();
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

  void CreateWidget() {
    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(100, 100, 200, 200);
    widget_->Init(std::move(params));
  }

 private:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  DISALLOW_COPY_AND_ASSIGN(HoverButtonTest);
};

class TestButtonListener : public views::ButtonListener {
 public:
  TestButtonListener() = default;
  ~TestButtonListener() override = default;

  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    last_sender_ = sender;
  }

  views::View* last_sender() { return last_sender_; }

 private:
  views::View* last_sender_ = nullptr;
  DISALLOW_COPY_AND_ASSIGN(TestButtonListener);
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
  for (size_t i = 0; i < base::size(kTitleSubtitlePairs); ++i) {
    TitleSubtitlePair pair = kTitleSubtitlePairs[i];
    SCOPED_TRACE(testing::Message() << "Index: " << i << ", expected_tooltip="
                                    << (pair.tooltip ? "true" : "false"));
    auto button = std::make_unique<HoverButton>(
        nullptr, CreateIcon(), base::ASCIIToUTF16(pair.title),
        base::ASCIIToUTF16(pair.subtitle));
    button->SetSize(gfx::Size(kButtonWidth, 40));

    ui::AXNodeData data;
    button->GetAccessibleNodeData(&data);
    std::string accessible_name;
    data.GetStringAttribute(ax::mojom::StringAttribute::kName,
                            &accessible_name);

    // The accessible name should always be the title and subtitle concatenated
    // by \n.
    base::string16 expected = base::JoinString(
        {base::ASCIIToUTF16(pair.title), base::ASCIIToUTF16(pair.subtitle)},
        base::ASCIIToUTF16("\n"));
    EXPECT_EQ(expected, base::UTF8ToUTF16(accessible_name));

    EXPECT_EQ(pair.tooltip ? expected : base::string16(),
              button->GetTooltipText(gfx::Point()));
  }
}

// Tests that setting a custom tooltip on a HoverButton will not be overwritten
// by HoverButton's own tooltips.
TEST_F(HoverButtonTest, CustomTooltip) {
  const base::string16 custom_tooltip = base::ASCIIToUTF16("custom");

  for (size_t i = 0; i < base::size(kTitleSubtitlePairs); ++i) {
    SCOPED_TRACE(testing::Message() << "Index: " << i);
    TitleSubtitlePair pair = kTitleSubtitlePairs[i];
    auto button = std::make_unique<HoverButton>(
        nullptr, CreateIcon(), base::ASCIIToUTF16(pair.title),
        base::ASCIIToUTF16(pair.subtitle));
    button->set_auto_compute_tooltip(false);
    button->SetTooltipText(custom_tooltip);
    button->SetSize(gfx::Size(kButtonWidth, 40));
    EXPECT_EQ(custom_tooltip, button->GetTooltipText(gfx::Point()));

    // Make sure the accessible name is still set.
    ui::AXNodeData data;
    button->GetAccessibleNodeData(&data);
    std::string accessible_name;
    data.GetStringAttribute(ax::mojom::StringAttribute::kName,
                            &accessible_name);

    // The accessible name should always be the title and subtitle concatenated
    // by \n.
    base::string16 expected = base::JoinString(
        {base::ASCIIToUTF16(pair.title), base::ASCIIToUTF16(pair.subtitle)},
        base::ASCIIToUTF16("\n"));
    EXPECT_EQ(expected, base::UTF8ToUTF16(accessible_name));
  }
}

// Tests that setting the style and the subtitle elide behavior don't lead to a
// crash for a HoverButton with an empty subtitle.
TEST_F(HoverButtonTest, SetStyleAndSubtitleElideBehavior) {
  HoverButton button(nullptr, CreateIcon(), base::ASCIIToUTF16("Test title"),
                     base::string16());
  button.SetStyle(HoverButton::STYLE_PROMINENT);
  button.SetSubtitleElideBehavior(gfx::ELIDE_EMAIL);
}

// Tests that a button with a subtitle and icons can be instantiated without a
// crash.
TEST_F(HoverButtonTest, CreateButtonWithSubtitleAndIcons) {
  std::unique_ptr<views::View> primary_icon = CreateIcon();
  views::View* primary_icon_raw = primary_icon.get();
  std::unique_ptr<views::View> secondary_icon = CreateIcon();
  views::View* secondary_icon_raw = secondary_icon.get();

  HoverButton button(nullptr, std::move(primary_icon),
                     base::ASCIIToUTF16("Title"),
                     base::ASCIIToUTF16("Subtitle"), std::move(secondary_icon));
  EXPECT_TRUE(button.Contains(primary_icon_raw));
  EXPECT_TRUE(button.Contains(secondary_icon_raw));
}

// Tests that the listener is notified on mouse release rather than mouse press.
TEST_F(HoverButtonTest, ActivatesOnMouseReleased) {
  TestButtonListener button_listener;
  HoverButton button(&button_listener, CreateIcon(),
                     base::ASCIIToUTF16("Title"), base::string16());

  button.SetBoundsRect(gfx::Rect(100, 100, 200, 200));
  widget()->SetContentsView(&button);
  widget()->Show();

  // ButtonListener should not be activated on press.
  generator()->PressLeftButton();
  EXPECT_EQ(nullptr, button_listener.last_sender());

  // ButtonListener should be activated on release.
  generator()->ReleaseLeftButton();
  EXPECT_EQ(&button, button_listener.last_sender());
}

}  // namespace
