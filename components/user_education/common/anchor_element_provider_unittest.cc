// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/anchor_element_provider.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"

namespace user_education {

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestElementId1);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestElementId2);
const ui::ElementContext kTestElementContext1(1);
const ui::ElementContext kTestElementContext2(2);
}  // namespace

TEST(AnchorElementProviderTest,
     AnchorElementProviderCommonRetrievesSingleElement) {
  ui::test::TestElement el(kTestElementId1, kTestElementContext1);

  AnchorElementProviderCommon provider(kTestElementId1);

  // No visible element, so returns null.
  EXPECT_EQ(nullptr, provider.GetAnchorElement(kTestElementContext1));
  el.Show();

  // Finds visible element.
  EXPECT_EQ(&el, provider.GetAnchorElement(kTestElementContext1));

  // Does not find element in the wrong context.
  EXPECT_EQ(nullptr, provider.GetAnchorElement(kTestElementContext2));

  // Wrong ID does not find element.
  AnchorElementProviderCommon provider2(kTestElementId2);
  EXPECT_EQ(nullptr, provider2.GetAnchorElement(kTestElementContext1));
}

}  // namespace user_education
