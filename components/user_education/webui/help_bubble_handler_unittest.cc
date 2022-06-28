// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "components/user_education/webui/help_bubble_webui.h"
#include "components/user_education/webui/tracked_element_webui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom.h"

namespace user_education {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kHelpBubbleHandlerTestElementIdentifier);

// Mock version of the help bubble client so we don't need a remote (while being
// able to know when the remote methods would have been called).
class MockHelpBubbleClient : public help_bubble::mojom::HelpBubbleClient {
 public:
  MockHelpBubbleClient() = default;
  ~MockHelpBubbleClient() override = default;

  MOCK_METHOD(void,
              ShowHelpBubble,
              (help_bubble::mojom::HelpBubbleParamsPtr data),
              (override));
  MOCK_METHOD(void, ToggleFocusForAccessibility, (), (override));
  MOCK_METHOD(void, HideHelpBubble, (), (override));
};

// Handler that mocks the remote connection to the web side of the component.
// The mock is a strict mock and can be retrieved by calling the `mock()`
// method.
class TestHelpBubbleHandler : public HelpBubbleHandlerBase {
 public:
  explicit TestHelpBubbleHandler(ui::ElementIdentifier identifier)
      : HelpBubbleHandlerBase(std::make_unique<ClientProvider>(),
                              identifier,
                              ui::ElementContext(this)) {}

  ~TestHelpBubbleHandler() override = default;

  // Provides direct access to the mock client for use with EXPECT_CALL*
  // macros.
  testing::StrictMock<MockHelpBubbleClient>& mock() {
    return static_cast<ClientProvider*>(client_provider())->client_;
  }

 private:
  // Provides a mock client.
  class ClientProvider : public HelpBubbleHandlerBase::ClientProvider {
   public:
    ClientProvider() = default;
    ~ClientProvider() override = default;

    help_bubble::mojom::HelpBubbleClient* GetClient() override {
      return &client_;
    }

   private:
    friend class TestHelpBubbleHandler;

    testing::StrictMock<MockHelpBubbleClient> client_;
  };
};

}  // namespace

// Tests the interaction of HelpBubbleHandler, HelpBubbleWebUI, and
// TrackedElementWebUI. The three form a single system that all work together.
class HelpBubbleHandlerTest : public testing::Test {
 public:
  HelpBubbleHandlerTest() {
    help_bubble_factory_registry_.MaybeRegister<HelpBubbleFactoryWebUI>();
  }
  ~HelpBubbleHandlerTest() override = default;

  void SetUp() override {
    test_handler_ = std::make_unique<TestHelpBubbleHandler>(
        kHelpBubbleHandlerTestElementIdentifier);
  }

 protected:
  help_bubble::mojom::HelpBubbleHandler* handler() {
    return test_handler_.get();
  }

  std::unique_ptr<TestHelpBubbleHandler> test_handler_;
  HelpBubbleFactoryRegistry help_bubble_factory_registry_;
};

TEST_F(HelpBubbleHandlerTest, StartsWithNoElement) {
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kHelpBubbleHandlerTestElementIdentifier));
}

TEST_F(HelpBubbleHandlerTest, ElementCreatedOnEvent) {
  handler()->HelpBubbleHostVisibilityChanged(true);
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kHelpBubbleHandlerTestElementIdentifier));

  // Verify that we don't leave elements dangling if the handler is destroyed.
  test_handler_.reset();
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kHelpBubbleHandlerTestElementIdentifier));
}

TEST_F(HelpBubbleHandlerTest, ElementHiddenOnEvent) {
  handler()->HelpBubbleHostVisibilityChanged(true);
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kHelpBubbleHandlerTestElementIdentifier));

  // Verify that we don't leave elements dangling if the handler is destroyed.
  handler()->HelpBubbleHostVisibilityChanged(false);
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kHelpBubbleHandlerTestElementIdentifier));
}

TEST_F(HelpBubbleHandlerTest, ShowHelpBubble) {
  handler()->HelpBubbleHostVisibilityChanged(true);
  auto* const element =
      ui::ElementTracker::GetElementTracker()->GetUniqueElement(
          kHelpBubbleHandlerTestElementIdentifier, test_handler_->context());
  ASSERT_NE(nullptr, element);
  HelpBubbleParams params;
  params.body_text = u"Help bubble body.";
  params.arrow = HelpBubbleArrow::kTopCenter;

  EXPECT_CALL(test_handler_->mock(), ShowHelpBubble(testing::_));
  auto help_bubble = help_bubble_factory_registry_.CreateHelpBubble(
      element, std::move(params));
  EXPECT_CALL(test_handler_->mock(), ShowHelpBubble(testing::_)).Times(0);

  EXPECT_TRUE(help_bubble);
  EXPECT_TRUE(help_bubble->is_open());

  EXPECT_CALL(test_handler_->mock(), HideHelpBubble());
  EXPECT_TRUE(help_bubble->Close());
  EXPECT_CALL(test_handler_->mock(), HideHelpBubble()).Times(0);

  EXPECT_FALSE(help_bubble->is_open());
}

TEST_F(HelpBubbleHandlerTest, FocusHelpBubble) {
  handler()->HelpBubbleHostVisibilityChanged(true);
  auto* const element =
      ui::ElementTracker::GetElementTracker()->GetUniqueElement(
          kHelpBubbleHandlerTestElementIdentifier, test_handler_->context());
  ASSERT_NE(nullptr, element);
  HelpBubbleParams params;
  params.body_text = u"Help bubble body.";
  params.arrow = HelpBubbleArrow::kTopCenter;

  EXPECT_CALL(test_handler_->mock(), ShowHelpBubble(testing::_));
  auto help_bubble = help_bubble_factory_registry_.CreateHelpBubble(
      element, std::move(params));

  EXPECT_CALL(test_handler_->mock(), ToggleFocusForAccessibility());
  help_bubble_factory_registry_.ToggleFocusForAccessibility(
      test_handler_->context());
  EXPECT_CALL(test_handler_->mock(), ToggleFocusForAccessibility()).Times(0);

  EXPECT_CALL(test_handler_->mock(), HideHelpBubble());
  EXPECT_TRUE(help_bubble->Close());
}

TEST_F(HelpBubbleHandlerTest, HelpBubbleClosedWhenVisibilityChanges) {
  handler()->HelpBubbleHostVisibilityChanged(true);
  auto* const element =
      ui::ElementTracker::GetElementTracker()->GetUniqueElement(
          kHelpBubbleHandlerTestElementIdentifier, test_handler_->context());
  ASSERT_NE(nullptr, element);
  HelpBubbleParams params;
  params.body_text = u"Help bubble body.";
  params.arrow = HelpBubbleArrow::kTopCenter;

  EXPECT_CALL(test_handler_->mock(), ShowHelpBubble(testing::_));
  auto help_bubble = help_bubble_factory_registry_.CreateHelpBubble(
      element, std::move(params));

  handler()->HelpBubbleHostVisibilityChanged(false);
  EXPECT_FALSE(help_bubble->is_open());
}

TEST_F(HelpBubbleHandlerTest, HelpBubbleClosedWhenClosedRemotely) {
  UNCALLED_MOCK_CALLBACK(HelpBubble::ClosedCallback, closed);

  handler()->HelpBubbleHostVisibilityChanged(true);
  auto* const element =
      ui::ElementTracker::GetElementTracker()->GetUniqueElement(
          kHelpBubbleHandlerTestElementIdentifier, test_handler_->context());
  ASSERT_NE(nullptr, element);
  HelpBubbleParams params;
  params.body_text = u"Help bubble body.";
  params.arrow = HelpBubbleArrow::kTopCenter;

  EXPECT_CALL(test_handler_->mock(), ShowHelpBubble(testing::_));
  auto help_bubble = help_bubble_factory_registry_.CreateHelpBubble(
      element, std::move(params));
  auto subscription = help_bubble->AddOnCloseCallback(closed.Get());

  EXPECT_CALL_IN_SCOPE(closed, Run, handler()->HelpBubbleClosed(false));
  EXPECT_FALSE(help_bubble->is_open());
}

TEST_F(HelpBubbleHandlerTest, DestroyHandlerClosesHelpBubble) {
  UNCALLED_MOCK_CALLBACK(HelpBubble::ClosedCallback, closed);

  handler()->HelpBubbleHostVisibilityChanged(true);
  auto* const element =
      ui::ElementTracker::GetElementTracker()->GetUniqueElement(
          kHelpBubbleHandlerTestElementIdentifier, test_handler_->context());
  ASSERT_NE(nullptr, element);
  HelpBubbleParams params;
  params.body_text = u"Help bubble body.";
  params.arrow = HelpBubbleArrow::kTopCenter;

  EXPECT_CALL(test_handler_->mock(), ShowHelpBubble(testing::_));
  auto help_bubble = help_bubble_factory_registry_.CreateHelpBubble(
      element, std::move(params));
  auto subscription = help_bubble->AddOnCloseCallback(closed.Get());

  EXPECT_CALL(test_handler_->mock(), HideHelpBubble());
  EXPECT_CALL_IN_SCOPE(closed, Run, test_handler_.reset());
  EXPECT_FALSE(help_bubble->is_open());
}

TEST_F(HelpBubbleHandlerTest, HelpBubbleClosedWhenClosedByUserCallsDismiss) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, dismissed);

  handler()->HelpBubbleHostVisibilityChanged(true);
  auto* const element =
      ui::ElementTracker::GetElementTracker()->GetUniqueElement(
          kHelpBubbleHandlerTestElementIdentifier, test_handler_->context());
  ASSERT_NE(nullptr, element);
  HelpBubbleParams params;
  params.body_text = u"Help bubble body.";
  params.arrow = HelpBubbleArrow::kTopCenter;
  params.dismiss_callback = dismissed.Get();

  EXPECT_CALL(test_handler_->mock(), ShowHelpBubble(testing::_));
  auto help_bubble = help_bubble_factory_registry_.CreateHelpBubble(
      element, std::move(params));

  EXPECT_CALL_IN_SCOPE(dismissed, Run, handler()->HelpBubbleClosed(true));
  EXPECT_FALSE(help_bubble->is_open());
}

TEST_F(HelpBubbleHandlerTest, ButtonPressedCallsCallback) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, button1_pressed);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, button2_pressed);

  handler()->HelpBubbleHostVisibilityChanged(true);
  auto* const element =
      ui::ElementTracker::GetElementTracker()->GetUniqueElement(
          kHelpBubbleHandlerTestElementIdentifier, test_handler_->context());
  ASSERT_NE(nullptr, element);

  HelpBubbleParams params;
  params.body_text = u"Help bubble body.";
  params.arrow = HelpBubbleArrow::kTopCenter;

  HelpBubbleButtonParams button1;
  button1.is_default = true;
  button1.text = u"button1";
  button1.callback = button1_pressed.Get();
  params.buttons.emplace_back(std::move(button1));

  HelpBubbleButtonParams button2;
  button2.is_default = false;
  button2.text = u"button2";
  button2.callback = button2_pressed.Get();
  params.buttons.emplace_back(std::move(button2));

  EXPECT_CALL(test_handler_->mock(), ShowHelpBubble(testing::_));
  auto help_bubble = help_bubble_factory_registry_.CreateHelpBubble(
      element, std::move(params));

  EXPECT_CALL_IN_SCOPE(button2_pressed, Run,
                       handler()->HelpBubbleButtonPressed(1));
  EXPECT_FALSE(help_bubble->is_open());
}

}  // namespace user_education
