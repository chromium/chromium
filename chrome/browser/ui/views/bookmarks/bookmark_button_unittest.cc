// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_button.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

class BookmarkButtonTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    set_native_widget_type(NativeWidgetType::kDesktop);
    ChromeViewsTestBase::SetUp();
  }

  void TearDown() override {
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  // Creates a BookmarkButton with the given |url| and |title| and hosts it
  // inside a widget so tooltip and accessibility APIs are functional.
  // Passes nullptr for browser since the tested code paths (tooltip
  // computation, SetText, AdjustAccessibleName) do not access it.
  BookmarkButton* CreateButtonInWidget(const GURL& url,
                                       std::u16string_view title) {
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

    auto button = std::make_unique<BookmarkButton>(
        views::Button::PressedCallback(), url, title, /*browser=*/nullptr);
    BookmarkButton* button_ptr = widget_->SetContentsView(std::move(button));
    widget_->Show();

    // Give the button a reasonable size so tooltip width calculations work.
    button_ptr->SetBounds(0, 0, 100, 30);
    return button_ptr;
  }

 private:
  std::unique_ptr<views::Widget> widget_;
};

// Verifies that a BookmarkButton can be constructed with a title and URL, and
// that the button text reflects the title.
TEST_F(BookmarkButtonTest, BasicConstruction) {
  GURL url("https://www.example.com");
  BookmarkButton* button = CreateButtonInWidget(url, u"Example");
  EXPECT_EQ(u"Example", button->GetText());
}

// Verifies that a BookmarkButton with an empty title still constructs without
// error.
TEST_F(BookmarkButtonTest, EmptyTitle) {
  GURL url("https://www.example.com");
  BookmarkButton* button = CreateButtonInWidget(url, u"");
  EXPECT_EQ(u"", button->GetText());
}

// Verifies that SetText() updates the button text.
TEST_F(BookmarkButtonTest, SetTextUpdatesLabel) {
  GURL url("https://www.example.com");
  BookmarkButton* button = CreateButtonInWidget(url, u"Old");
  EXPECT_EQ(u"Old", button->GetText());

  button->SetText(u"New");
  EXPECT_EQ(u"New", button->GetText());
}

// Verifies that tooltip text is not empty after the button is added to a
// widget (the tooltip is lazily computed but should be available via
// GetRenderedTooltipText).
TEST_F(BookmarkButtonTest, TooltipTextAvailableAfterAddedToWidget) {
  GURL url("https://www.example.com/page");
  BookmarkButton* button = CreateButtonInWidget(url, u"Example Page");

  // GetRenderedTooltipText triggers lazy tooltip computation.
  std::u16string tooltip = button->GetRenderedTooltipText(gfx::Point());
  EXPECT_FALSE(tooltip.empty());
}

// Verifies that the tooltip contains the URL.
TEST_F(BookmarkButtonTest, TooltipContainsURL) {
  GURL url("https://www.example.com/path");
  BookmarkButton* button = CreateButtonInWidget(url, u"My Bookmark");

  std::u16string tooltip = button->GetRenderedTooltipText(gfx::Point());
  EXPECT_NE(std::u16string::npos, tooltip.find(u"example.com"));
}

// Verifies that calling SetText invalidates the cached tooltip so that a
// subsequent GetRenderedTooltipText returns updated content.
TEST_F(BookmarkButtonTest, SetTextInvalidatesTooltip) {
  GURL url("https://www.example.com");
  BookmarkButton* button = CreateButtonInWidget(url, u"Original Title");

  // Force initial tooltip computation.
  std::u16string tooltip_before = button->GetRenderedTooltipText(gfx::Point());

  // Change the text; the tooltip should update to reflect the new title.
  button->SetText(u"Updated Title");
  std::u16string tooltip_after = button->GetRenderedTooltipText(gfx::Point());
  EXPECT_NE(tooltip_before, tooltip_after);
  EXPECT_NE(std::u16string::npos, tooltip_after.find(u"Updated Title"));
}

// Verifies that the tooltip updates when the button text changes from a
// non-empty to an empty title.
TEST_F(BookmarkButtonTest, TooltipUpdatesOnEmptyTitle) {
  GURL url("https://www.example.com");
  BookmarkButton* button = CreateButtonInWidget(url, u"Has Title");

  std::u16string tooltip_with_title =
      button->GetRenderedTooltipText(gfx::Point());

  button->SetText(u"");
  std::u16string tooltip_without_title =
      button->GetRenderedTooltipText(gfx::Point());

  // Both should contain the URL, but the text portion should differ.
  EXPECT_NE(tooltip_with_title, tooltip_without_title);
}

// Verifies that AdjustAccessibleName provides a fallback name when the button
// title is empty, so screen readers have something useful to announce.
TEST_F(BookmarkButtonTest, AccessibleNameFallbackForEmptyTitle) {
  GURL url("https://www.example.com");
  BookmarkButton* button = CreateButtonInWidget(url, u"");

  std::u16string name;
  ax::mojom::NameFrom name_from = ax::mojom::NameFrom::kNone;
  button->AdjustAccessibleName(name, name_from);

  // The fallback name should contain the URL.
  EXPECT_FALSE(name.empty());
  EXPECT_NE(std::u16string::npos, name.find(u"example.com"));
  EXPECT_EQ(ax::mojom::NameFrom::kContents, name_from);
}

// Verifies that AdjustAccessibleName does NOT replace a non-empty name.
TEST_F(BookmarkButtonTest, AccessibleNamePreservedWhenNonEmpty) {
  GURL url("https://www.example.com");
  BookmarkButton* button = CreateButtonInWidget(url, u"My Bookmark");

  std::u16string name = u"My Bookmark";
  ax::mojom::NameFrom name_from = ax::mojom::NameFrom::kAttribute;
  button->AdjustAccessibleName(name, name_from);

  EXPECT_EQ(u"My Bookmark", name);
  // name_from should remain unchanged when the name is not empty.
  EXPECT_EQ(ax::mojom::NameFrom::kAttribute, name_from);
}

// Verifies that SetTooltipText (called during tooltip computation) updates the
// accessible description so screen readers can announce it.
TEST_F(BookmarkButtonTest, TooltipSetsAccessibleDescription) {
  GURL url("https://www.example.com/page");
  BookmarkButton* button = CreateButtonInWidget(url, u"Example");

  // Trigger tooltip computation.
  button->GetRenderedTooltipText(gfx::Point());

  ui::AXNodeData data;
  button->GetViewAccessibility().GetAccessibleNodeData(&data);
  std::u16string description = base::ASCIIToUTF16(
      data.GetStringAttribute(ax::mojom::StringAttribute::kDescription));
  EXPECT_FALSE(description.empty());
}

}  // namespace
