// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/help_bubble_factory_registry.h"

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/test/test_help_bubble.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/expect_call_in_scope.h"

namespace user_education {

namespace {

// Placeholder ID and context for test elements.
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestElementIdentifier);
const ui::ElementContext kTestElementContext(1U);
const ui::ElementContext kTestElementContext2(2U);

}  // namespace

class HelpBubbleFactoryRegistryTest : public testing::Test {
 public:
  HelpBubbleFactoryRegistryTest()
      : test_element_(kTestElementIdentifier, kTestElementContext) {
    help_bubble_factory_registry_.MaybeRegister<test::TestHelpBubbleFactory>();
  }

  ~HelpBubbleFactoryRegistryTest() override = default;

  void TearDown() override {
    // By the time we get to tear-down, all bubbles should be closed.
    ASSERT_FALSE(help_bubble_factory_registry_.is_any_bubble_showing());
  }

 protected:
  HelpBubbleParams GetBubbleParams() {
    HelpBubbleParams params;
    params.body_text = u"To X, do Y";
    params.arrow = HelpBubbleArrow::kTopRight;
    return params;
  }

  HelpBubbleFactoryRegistry help_bubble_factory_registry_;
  ui::test::TestElement test_element_;
};

TEST_F(HelpBubbleFactoryRegistryTest, ShowsBubble) {
  EXPECT_FALSE(help_bubble_factory_registry_.is_any_bubble_showing());

  auto bubble = help_bubble_factory_registry_.CreateHelpBubble(
      &test_element_, GetBubbleParams());
  ASSERT_TRUE(bubble);
  EXPECT_TRUE(bubble->is_open());
  EXPECT_TRUE(help_bubble_factory_registry_.is_any_bubble_showing());

  bubble.reset();
  EXPECT_FALSE(help_bubble_factory_registry_.is_any_bubble_showing());
}

TEST_F(HelpBubbleFactoryRegistryTest, ShowSecondBubble) {
  auto bubble = help_bubble_factory_registry_.CreateHelpBubble(
      &test_element_, GetBubbleParams());
  auto bubble2 = help_bubble_factory_registry_.CreateHelpBubble(
      &test_element_, GetBubbleParams());

  EXPECT_TRUE(bubble);
  EXPECT_TRUE(bubble2);
  EXPECT_TRUE(help_bubble_factory_registry_.is_any_bubble_showing());
  bubble.reset();
  EXPECT_TRUE(help_bubble_factory_registry_.is_any_bubble_showing());
  bubble2.reset();
  EXPECT_FALSE(help_bubble_factory_registry_.is_any_bubble_showing());
}

TEST_F(HelpBubbleFactoryRegistryTest, DeleteBubble) {
  auto bubble = help_bubble_factory_registry_.CreateHelpBubble(
      &test_element_, GetBubbleParams());
  bubble.reset();
  EXPECT_FALSE(help_bubble_factory_registry_.is_any_bubble_showing());
}

TEST_F(HelpBubbleFactoryRegistryTest, DeleteTwoBubbles) {
  auto bubble = help_bubble_factory_registry_.CreateHelpBubble(
      &test_element_, GetBubbleParams());
  auto bubble2 = help_bubble_factory_registry_.CreateHelpBubble(
      &test_element_, GetBubbleParams());
  bubble.reset();
  EXPECT_TRUE(help_bubble_factory_registry_.is_any_bubble_showing());
  bubble2.reset();
  EXPECT_FALSE(help_bubble_factory_registry_.is_any_bubble_showing());
}

TEST_F(HelpBubbleFactoryRegistryTest, CloseBubble) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, close_callback);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, timeout_callback);
  auto params = GetBubbleParams();
  params.dismiss_callback = close_callback.Get();
  params.timeout_callback = timeout_callback.Get();
  auto bubble = help_bubble_factory_registry_.CreateHelpBubble(
      &test_element_, std::move(params));

  bubble->Close();
  EXPECT_FALSE(bubble->is_open());
  EXPECT_FALSE(help_bubble_factory_registry_.is_any_bubble_showing());
}

TEST_F(HelpBubbleFactoryRegistryTest, DismissBubble) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, close_callback);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, timeout_callback);
  auto params = GetBubbleParams();
  params.dismiss_callback = close_callback.Get();
  params.timeout_callback = timeout_callback.Get();
  auto bubble = help_bubble_factory_registry_.CreateHelpBubble(
      &test_element_, std::move(params));
  auto* const test_bubble = static_cast<test::TestHelpBubble*>(bubble.get());

  EXPECT_CALL_IN_SCOPE(close_callback, Run, test_bubble->SimulateDismiss());
  EXPECT_FALSE(bubble->is_open());
  EXPECT_FALSE(help_bubble_factory_registry_.is_any_bubble_showing());
}

TEST_F(HelpBubbleFactoryRegistryTest, BubbleTimeout) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, close_callback);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, timeout_callback);
  auto params = GetBubbleParams();
  params.dismiss_callback = close_callback.Get();
  params.timeout_callback = timeout_callback.Get();
  auto bubble = help_bubble_factory_registry_.CreateHelpBubble(
      &test_element_, std::move(params));
  auto* const test_bubble = static_cast<test::TestHelpBubble*>(bubble.get());

  EXPECT_CALL_IN_SCOPE(timeout_callback, Run, test_bubble->SimulateTimeout());
  EXPECT_FALSE(bubble->is_open());
  EXPECT_FALSE(help_bubble_factory_registry_.is_any_bubble_showing());
}

TEST_F(HelpBubbleFactoryRegistryTest, PressButton) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, close_callback);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, timeout_callback);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, button_callback);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, button2_callback);
  auto params = GetBubbleParams();
  params.dismiss_callback = close_callback.Get();
  params.timeout_callback = timeout_callback.Get();
  HelpBubbleButtonParams button_params;
  button_params.text = u"press me";
  button_params.is_default = true;
  button_params.callback = button_callback.Get();
  params.buttons.push_back(std::move(button_params));
  HelpBubbleButtonParams button2_params;
  button2_params.text = u"other";
  button2_params.is_default = false;
  button2_params.callback = button2_callback.Get();
  params.buttons.push_back(std::move(button2_params));
  auto bubble = help_bubble_factory_registry_.CreateHelpBubble(
      &test_element_, std::move(params));
  auto* const test_bubble = static_cast<test::TestHelpBubble*>(bubble.get());

  EXPECT_CALL_IN_SCOPE(button_callback, Run,
                       test_bubble->SimulateButtonPress(0));
}

TEST_F(HelpBubbleFactoryRegistryTest, PressButton2) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, close_callback);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, timeout_callback);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, button_callback);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, button2_callback);
  auto params = GetBubbleParams();
  params.dismiss_callback = close_callback.Get();
  params.timeout_callback = timeout_callback.Get();
  HelpBubbleButtonParams button_params;
  button_params.text = u"press me";
  button_params.is_default = true;
  button_params.callback = button_callback.Get();
  params.buttons.push_back(std::move(button_params));
  HelpBubbleButtonParams button2_params;
  button2_params.text = u"other";
  button2_params.is_default = false;
  button2_params.callback = button2_callback.Get();
  params.buttons.push_back(std::move(button2_params));
  auto bubble = help_bubble_factory_registry_.CreateHelpBubble(
      &test_element_, std::move(params));
  auto* const test_bubble = static_cast<test::TestHelpBubble*>(bubble.get());

  EXPECT_CALL_IN_SCOPE(button2_callback, Run,
                       test_bubble->SimulateButtonPress(1));
}

TEST_F(HelpBubbleFactoryRegistryTest, ToggleFocusForAccessibility) {
  auto bubble = help_bubble_factory_registry_.CreateHelpBubble(
      &test_element_, GetBubbleParams());
  ASSERT_TRUE(bubble);
  auto* const test_bubble = static_cast<test::TestHelpBubble*>(bubble.get());
  EXPECT_EQ(0, test_bubble->focus_count());
  const bool result = help_bubble_factory_registry_.ToggleFocusForAccessibility(
      kTestElementContext);
  EXPECT_TRUE(result);
  EXPECT_EQ(1, test_bubble->focus_count());
}

TEST_F(HelpBubbleFactoryRegistryTest,
       ToggleFocusForAccessibility_WrongContext) {
  auto bubble = help_bubble_factory_registry_.CreateHelpBubble(
      &test_element_, GetBubbleParams());
  ASSERT_TRUE(bubble);
  auto* const test_bubble = static_cast<test::TestHelpBubble*>(bubble.get());
  EXPECT_EQ(0, test_bubble->focus_count());
  const bool result = help_bubble_factory_registry_.ToggleFocusForAccessibility(
      kTestElementContext2);
  EXPECT_FALSE(result);
  EXPECT_EQ(0, test_bubble->focus_count());
}

TEST_F(HelpBubbleFactoryRegistryTest, CloseTwoBubbles) {
  auto bubble = help_bubble_factory_registry_.CreateHelpBubble(
      &test_element_, GetBubbleParams());
  auto bubble2 = help_bubble_factory_registry_.CreateHelpBubble(
      &test_element_, GetBubbleParams());
  bubble->Close();
  EXPECT_TRUE(help_bubble_factory_registry_.is_any_bubble_showing());
  bubble2->Close();
  EXPECT_FALSE(help_bubble_factory_registry_.is_any_bubble_showing());
}

TEST_F(HelpBubbleFactoryRegistryTest, OpenSecondBubbleAfterClose) {
  auto bubble = help_bubble_factory_registry_.CreateHelpBubble(
      &test_element_, GetBubbleParams());
  EXPECT_TRUE(help_bubble_factory_registry_.is_any_bubble_showing());
  bubble->Close();
  EXPECT_FALSE(help_bubble_factory_registry_.is_any_bubble_showing());
  auto bubble2 = help_bubble_factory_registry_.CreateHelpBubble(
      &test_element_, GetBubbleParams());
  EXPECT_TRUE(help_bubble_factory_registry_.is_any_bubble_showing());
  bubble2->Close();
  EXPECT_FALSE(help_bubble_factory_registry_.is_any_bubble_showing());
}

}  // namespace user_education
