// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/pin_textfield.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/views_test_base.h"

namespace {

using PinTextfieldTest = views::ViewsTestBase;

TEST_F(PinTextfieldTest, AccessibleValue) {
  auto pin_textfield =
      std::make_unique<PinTextfield>(/* pin_digits_amount= */ 6);

  ui::AXNodeData data;
  pin_textfield->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kValue),
            std::u16string(u""));

  pin_textfield->SetObscured(false);
  pin_textfield->SetDisabled(false);
  pin_textfield->SetPin(u"12345");
  data = ui::AXNodeData();
  pin_textfield->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kValue),
            std::u16string(u"12345"));

  EXPECT_TRUE(pin_textfield->AppendDigit(u"6"));
  data = ui::AXNodeData();
  pin_textfield->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kValue),
            std::u16string(u"123456"));

  EXPECT_TRUE(pin_textfield->RemoveDigit());
  data = ui::AXNodeData();
  pin_textfield->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kValue),
            std::u16string(u"12345"));

  pin_textfield->SetObscured(true);
  data = ui::AXNodeData();
  pin_textfield->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kValue),
            std::u16string(/* pin_digits_amount= */ 5,
                           gfx::RenderText::kPasswordReplacementChar));

  pin_textfield->SetObscured(false);
  pin_textfield->SetDisabled(true);
  pin_textfield->SetPin(u"12345");  // SetDisabled(true) resets the Pin.
  data = ui::AXNodeData();
  pin_textfield->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kValue),
            std::u16string(/* pin_digits_amount= */ 5,
                           gfx::RenderText::kPasswordReplacementChar));
}

}  // namespace
