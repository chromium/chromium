// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/controls/rich_hover_button.h"

#include <memory>
#include <string>
#include <string_view>

#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/vector_icons.h"

namespace {

constexpr char16_t kTitle[] = u"Title";
constexpr char16_t kSubtitle[] = u"Subtitle";
constexpr char16_t kIconName[] = u"Icon description";
constexpr char16_t kActionIconName[] = u"Action icon description";

constexpr char16_t kFullName[] =
    u"Icon description\nTitle\nSubtitle\nAction icon description";

ui::ImageModel TestIcon() {
  return ui::ImageModel::FromVectorIcon(views::kInfoIcon);
}

ui::ImageModel OtherTestIcon() {
  return ui::ImageModel::FromVectorIcon(views::kCheckIcon);
}

// Returns a button with both labels and both icons set, but no icon
// descriptions.
std::unique_ptr<RichHoverButton> MakeButton(
    std::u16string_view subtitle = kSubtitle) {
  return std::make_unique<RichHoverButton>(views::Button::PressedCallback(),
                                           TestIcon(), kTitle, subtitle,
                                           TestIcon());
}

}  // namespace

using RichHoverButtonTest = ChromeViewsTestBase;

// The icons inside a `RichHoverButton` are not focusable, so a screen reader
// only reaches them if the user explicitly navigates into the button. Their
// descriptions must therefore be folded into the button's own accessible name,
// in visual reading order.
TEST_F(RichHoverButtonTest, AccessibleNameIncludesIconDescriptions) {
  std::unique_ptr<RichHoverButton> button = MakeButton();
  button->SetIconAccessibleName(kIconName);
  button->SetActionIconAccessibleName(kActionIconName);

  EXPECT_EQ(button->GetViewAccessibility().GetCachedName(), kFullName);
}

// Without icon descriptions the name is just the labels, with no stray
// separators.
TEST_F(RichHoverButtonTest, AccessibleNameWithoutIconDescriptions) {
  std::unique_ptr<RichHoverButton> button = MakeButton();

  EXPECT_EQ(button->GetViewAccessibility().GetCachedName(),
            std::u16string(u"Title\nSubtitle"));
}

TEST_F(RichHoverButtonTest, AccessibleNameWithoutSubtitle) {
  std::unique_ptr<RichHoverButton> button = MakeButton(std::u16string_view());
  button->SetIconAccessibleName(kIconName);
  button->SetActionIconAccessibleName(kActionIconName);

  EXPECT_EQ(
      button->GetViewAccessibility().GetCachedName(),
      std::u16string(u"Icon description\nTitle\nAction icon description"));
}

// The accessible name must not depend on the order in which the descriptions
// and the labels are set.
TEST_F(RichHoverButtonTest, AccessibleNameIndependentOfSetterOrder) {
  auto button = std::make_unique<RichHoverButton>();
  button->SetIconAccessibleName(kIconName);
  button->SetActionIconAccessibleName(kActionIconName);
  button->SetIcon(TestIcon());
  button->SetActionIcon(TestIcon());
  button->SetTitleText(kTitle);
  button->SetSubtitleText(kSubtitle);

  EXPECT_EQ(button->GetViewAccessibility().GetCachedName(), kFullName);
}

// Setting an icon destroys and recreates the underlying `ImageView`, so the
// icon descriptions must be retained on the button rather than on the child
// view.
TEST_F(RichHoverButtonTest, AccessibleNameSurvivesIconChange) {
  std::unique_ptr<RichHoverButton> button = MakeButton();
  button->SetIconAccessibleName(kIconName);
  button->SetActionIconAccessibleName(kActionIconName);

  button->SetIcon(OtherTestIcon());
  button->SetActionIcon(OtherTestIcon());

  EXPECT_EQ(button->GetViewAccessibility().GetCachedName(), kFullName);
}

// Changing the labels afterwards must keep the icon descriptions.
TEST_F(RichHoverButtonTest, AccessibleNameUpdatesWithLabels) {
  std::unique_ptr<RichHoverButton> button = MakeButton();
  button->SetIconAccessibleName(kIconName);
  button->SetActionIconAccessibleName(kActionIconName);

  button->SetTitleText(u"New title");
  button->SetSubtitleText(u"New subtitle");

  EXPECT_EQ(button->GetViewAccessibility().GetCachedName(),
            std::u16string(u"Icon description\nNew title\nNew subtitle\nAction "
                           u"icon description"));
}

// Clearing a description drops it from the name rather than leaving an empty
// line behind.
TEST_F(RichHoverButtonTest, AccessibleNameDropsClearedDescriptions) {
  std::unique_ptr<RichHoverButton> button = MakeButton();
  button->SetIconAccessibleName(kIconName);
  button->SetActionIconAccessibleName(kActionIconName);
  ASSERT_EQ(button->GetViewAccessibility().GetCachedName(), kFullName);

  button->SetIconAccessibleName(std::u16string());
  button->SetActionIconAccessibleName(std::u16string());

  EXPECT_EQ(button->GetViewAccessibility().GetCachedName(),
            std::u16string(u"Title\nSubtitle"));
}
