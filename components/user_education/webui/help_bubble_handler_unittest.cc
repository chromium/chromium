// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/webui/help_bubble_handler.h"

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/test/test_help_bubble.h"
#include "components/user_education/webui/help_bubble_webui.h"
#include "components/user_education/webui/tracked_element_webui.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_ui_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom-forward.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom-shared.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom.h"

namespace user_education {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kHelpBubbleHandlerTestElementIdentifier);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kHelpBubbleHandlerTestElementIdentifier2);
constexpr gfx::RectF kElementBounds{50, 51, 51, 53};

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
  MOCK_METHOD(void,
              ToggleFocusForAccessibility,
              (const std::string& native_identifier),
              (override));
  MOCK_METHOD(void,
              HideHelpBubble,
              (const std::string& native_identifier),
              (override));
  MOCK_METHOD(void,
              ExternalHelpBubbleUpdated,
              (const std::string& native_identifier, bool shown),
              (override));
};

// Handler that mocks the remote connection to the web side of the component.
// The mock is a strict mock and can be retrieved by calling the `mock()`
// method.
class TestHelpBubbleHandler : public HelpBubbleHandlerBase {
 public:
  explicit TestHelpBubbleHandler(
      const std::vector<ui::ElementIdentifier>& identifiers,
      std::unique_ptr<VisibilityProvider> visibility_provider)
      : HelpBubbleHandlerBase(std::make_unique<ClientProvider>(),
                              std::move(visibility_provider),
                              identifiers,
                              ui::ElementContext(this)) {}

  ~TestHelpBubbleHandler() override = default;

  // Provides direct access to the mock client for use with EXPECT_CALL*
  // macros.
  testing::StrictMock<MockHelpBubbleClient>& mock() {
    return static_cast<ClientProvider*>(client_provider())->client_;
  }

  content::WebUIController* GetController() override {
    NOTREACHED_IN_MIGRATION() << "Should not call this in tests.";
    return nullptr;
  }

  class MockVisibilityProvider
      : public HelpBubbleHandlerBase::VisibilityProvider {
   public:
    MockVisibilityProvider() = default;
    ~MockVisibilityProvider() override = default;

    using HelpBubbleHandlerBase::VisibilityProvider::SetLastKnownVisibility;

    MOCK_METHOD(std::optional<bool>, CheckIsVisible, (), (override));
  };

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

MATCHER_P(MatchesHelpBubbleParams, expected, "") {
  EXPECT_EQ(expected->body_text, arg->body_text);
  EXPECT_EQ(expected->close_button_alt_text, arg->close_button_alt_text);
  EXPECT_EQ(expected->timeout, arg->timeout);
  EXPECT_EQ(expected->body_icon_name, arg->body_icon_name);
  EXPECT_EQ(expected->body_icon_alt_text, arg->body_icon_alt_text);
  EXPECT_EQ(expected->native_identifier, arg->native_identifier);
  EXPECT_EQ(expected->position, arg->position);
  EXPECT_EQ(expected->title_text, arg->title_text);
  EXPECT_EQ(!!expected->progress, !!arg->progress);
  if (expected->progress && arg->progress) {
    EXPECT_EQ(expected->progress->current, arg->progress->current);
    EXPECT_EQ(expected->progress->total, arg->progress->total);
  }

  EXPECT_EQ(expected->buttons.size(), arg->buttons.size());
  if (expected->buttons.size() == arg->buttons.size()) {
    for (size_t i = 0; i < expected->buttons.size(); ++i) {
      EXPECT_EQ(expected->buttons[i]->text, arg->buttons[i]->text);
      EXPECT_EQ(expected->buttons[i]->is_default, arg->buttons[i]->is_default);
    }
  }

  return true;
}

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
    auto visibility_provider =
        std::make_unique<TestHelpBubbleHandler::MockVisibilityProvider>();
    EXPECT_CALL(*visibility_provider.get(), CheckIsVisible)
        .WillRepeatedly(testing::Return(true));
    visibility_provider_ = visibility_provider.get();
    test_handler_ = std::make_unique<TestHelpBubbleHandler>(
        std::vector{kHelpBubbleHandlerTestElementIdentifier,
                    kHelpBubbleHandlerTestElementIdentifier2},
        std::move(visibility_provider));
  }

  void TearDown() override { visibility_provider_ = nullptr; }

 protected:
  help_bubble::mojom::HelpBubbleHandler* handler() {
    return test_handler_.get();
  }

  raw_ptr<TestHelpBubbleHandler::MockVisibilityProvider, DanglingUntriaged>
      visibility_provider_ = nullptr;
  std::unique_ptr<TestHelpBubbleHandler> test_handler_;
  HelpBubbleFactoryRegistry help_bubble_factory_registry_;
};

TEST_F(HelpBubbleHandlerTest, StartsWithNoElement) {
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kHelpBubbleHandlerTestElementIdentifier));
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kHelpBubbleHandlerTestElementIdentifier2));
}

TEST_F(HelpBubbleHandlerTest, ElementCreatedOnEvent) {
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier.GetName(), true, kElementBounds);
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kHelpBubbleHandlerTestElementIdentifier));
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kHelpBubbleHandlerTestElementIdentifier2));

  // Verify that we don't leave elements dangling if the handler is destroyed.
  test_handler_.reset();
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kHelpBubbleHandlerTestElementIdentifier));
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kHelpBubbleHandlerTestElementIdentifier2));
}

TEST_F(HelpBubbleHandlerTest, ElementHiddenOnEvent) {
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier.GetName(), true, kElementBounds);
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier.GetName(), false, gfx::RectF());
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kHelpBubbleHandlerTestElementIdentifier));
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kHelpBubbleHandlerTestElementIdentifier2));
}

TEST_F(HelpBubbleHandlerTest, ElementActivatedOnEvent) {
  UNCALLED_MOCK_CALLBACK(ui::ElementTracker::Callback, activated);
  const std::string name = kHelpBubbleHandlerTestElementIdentifier.GetName();
  handler()->HelpBubbleAnchorVisibilityChanged(name, true, kElementBounds);
  auto* const tracker = ui::ElementTracker::GetElementTracker();
  auto* const element =
      tracker->GetElementInAnyContext(kHelpBubbleHandlerTestElementIdentifier);
  auto subscription =
      ui::ElementTracker::GetElementTracker()->AddElementActivatedCallback(
          element->identifier(), element->context(), activated.Get());
  EXPECT_CALL_IN_SCOPE(activated, Run(element),
                       handler()->HelpBubbleAnchorActivated(name));
}

TEST_F(HelpBubbleHandlerTest, ElementCustomEventOnEvent) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kCustomEvent);
  const std::string event_name = kCustomEvent.GetName();
  const std::string element_name =
      kHelpBubbleHandlerTestElementIdentifier.GetName();
  UNCALLED_MOCK_CALLBACK(ui::ElementTracker::Callback, custom_event);
  handler()->HelpBubbleAnchorVisibilityChanged(element_name, true,
                                               kElementBounds);
  auto* const tracker = ui::ElementTracker::GetElementTracker();
  auto* const element =
      tracker->GetElementInAnyContext(kHelpBubbleHandlerTestElementIdentifier);
  auto subscription =
      ui::ElementTracker::GetElementTracker()->AddCustomEventCallback(
          kCustomEvent, element->context(), custom_event.Get());
  EXPECT_CALL_IN_SCOPE(
      custom_event, Run(element),
      handler()->HelpBubbleAnchorCustomEvent(element_name, event_name));
}

TEST_F(HelpBubbleHandlerTest, MultipleIdentifiers) {
  // Show two elements.
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier.GetName(), true, kElementBounds);
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier2.GetName(), true, kElementBounds);
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kHelpBubbleHandlerTestElementIdentifier));
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kHelpBubbleHandlerTestElementIdentifier2));

  // Hide one element.
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier.GetName(), false, gfx::RectF());
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kHelpBubbleHandlerTestElementIdentifier));
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kHelpBubbleHandlerTestElementIdentifier2));

  // Hide the other element.
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier2.GetName(), false, gfx::RectF());
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kHelpBubbleHandlerTestElementIdentifier));
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kHelpBubbleHandlerTestElementIdentifier2));

  // Re-show an element.
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier.GetName(), true, kElementBounds);
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kHelpBubbleHandlerTestElementIdentifier));
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kHelpBubbleHandlerTestElementIdentifier2));
}

TEST_F(HelpBubbleHandlerTest, ShowHelpBubble) {
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier.GetName(), true, kElementBounds);
  auto* const element =
      ui::ElementTracker::GetElementTracker()->GetUniqueElement(
          kHelpBubbleHandlerTestElementIdentifier, test_handler_->context());
  ASSERT_NE(nullptr, element);
  HelpBubbleParams params;
  params.body_text = u"Help bubble body.";
  params.close_button_alt_text = u"Close button alt text.";
  params.body_icon = &vector_icons::kCelebrationIcon;
  params.body_icon_alt_text = u"Celebration";
  params.arrow = HelpBubbleArrow::kTopCenter;

  // Check the parameters passed to the ShowHelpBubble mojo method.
  help_bubble::mojom::HelpBubbleParamsPtr expected =
      help_bubble::mojom::HelpBubbleParams::New();
  expected->native_identifier = element->identifier().GetName();
  expected->body_text = base::UTF16ToUTF8(params.body_text);
  expected->close_button_alt_text =
      base::UTF16ToUTF8(params.close_button_alt_text);
  expected->body_icon_name = "celebration";
  expected->body_icon_alt_text = "Celebration";
  expected->position = help_bubble::mojom::HelpBubbleArrowPosition::TOP_CENTER;
  expected->timeout = base::Seconds(10);

  EXPECT_CALL(test_handler_->mock(),
              ShowHelpBubble(MatchesHelpBubbleParams(expected.get())));
  auto help_bubble = help_bubble_factory_registry_.CreateHelpBubble(
      element, std::move(params));
  EXPECT_CALL(test_handler_->mock(), ShowHelpBubble(testing::_)).Times(0);

  EXPECT_TRUE(help_bubble);
  EXPECT_TRUE(help_bubble->is_open());

  EXPECT_CALL(
      test_handler_->mock(),
      HideHelpBubble(kHelpBubbleHandlerTestElementIdentifier.GetName()));
  EXPECT_TRUE(help_bubble->Close());
  EXPECT_CALL(test_handler_->mock(), HideHelpBubble).Times(0);

  EXPECT_FALSE(help_bubble->is_open());
}

// Regression test for possible cause of crbug.com/1474307.
TEST_F(HelpBubbleHandlerTest, ShowHelpBubbleTwice) {
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier.GetName(), true, kElementBounds);
  auto* const element =
      ui::ElementTracker::GetElementTracker()->GetUniqueElement(
          kHelpBubbleHandlerTestElementIdentifier, test_handler_->context());
  ASSERT_NE(nullptr, element);

  auto get_params = []() {
    HelpBubbleParams params;
    params.body_text = u"Help bubble body.";
    params.close_button_alt_text = u"Close button alt text.";
    params.body_icon = &vector_icons::kCelebrationIcon;
    params.body_icon_alt_text = u"Celebration";
    params.arrow = HelpBubbleArrow::kTopCenter;
    return params;
  };

  EXPECT_CALL(test_handler_->mock(), ShowHelpBubble(testing::_));
  auto help_bubble =
      help_bubble_factory_registry_.CreateHelpBubble(element, get_params());
  EXPECT_CALL(test_handler_->mock(), HideHelpBubble(testing::_));
  EXPECT_CALL(test_handler_->mock(), ShowHelpBubble(testing::_));
  auto help_bubble2 =
      help_bubble_factory_registry_.CreateHelpBubble(element, get_params());
  EXPECT_CALL(test_handler_->mock(), ShowHelpBubble(testing::_)).Times(0);
  EXPECT_CALL(test_handler_->mock(), HideHelpBubble(testing::_));

  EXPECT_FALSE(help_bubble->is_open());
  EXPECT_TRUE(help_bubble2->is_open());
}

TEST_F(HelpBubbleHandlerTest, ShowHelpBubbleWithButtonsAndProgress) {
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier.GetName(), true, kElementBounds);
  auto* const element =
      ui::ElementTracker::GetElementTracker()->GetUniqueElement(
          kHelpBubbleHandlerTestElementIdentifier, test_handler_->context());
  ASSERT_NE(nullptr, element);
  HelpBubbleParams params;
  params.body_text = u"Help bubble body.";
  params.close_button_alt_text = u"Close button alt text.";
  params.body_icon = &vector_icons::kLightbulbOutlineIcon;
  params.body_icon_alt_text = u"Body icon alt text.";
  params.arrow = HelpBubbleArrow::kTopCenter;
  params.progress = std::make_pair(1, 3);

  HelpBubbleButtonParams button;
  button.is_default = true;
  button.text = u"button1";
  params.buttons.emplace_back(std::move(button));

  // Check the parameters passed to the ShowHelpBubble mojo method.
  help_bubble::mojom::HelpBubbleParamsPtr expected =
      help_bubble::mojom::HelpBubbleParams::New();
  expected->native_identifier = element->identifier().GetName();
  expected->body_text = base::UTF16ToUTF8(params.body_text);
  expected->close_button_alt_text =
      base::UTF16ToUTF8(params.close_button_alt_text);
  expected->body_icon_name = "lightbulb_outline";
  expected->body_icon_alt_text = "Body icon alt text.";
  expected->position = help_bubble::mojom::HelpBubbleArrowPosition::TOP_CENTER;

  auto expected_button = help_bubble::mojom::HelpBubbleButtonParams::New();
  expected_button->text = "button1";
  expected_button->is_default = true;
  expected->buttons.emplace_back(std::move(expected_button));

  auto expected_progress = help_bubble::mojom::Progress::New();
  expected_progress->current = 1;
  expected_progress->total = 3;
  expected->progress = std::move(expected_progress);

  EXPECT_CALL(test_handler_->mock(),
              ShowHelpBubble(MatchesHelpBubbleParams(expected.get())));
  auto help_bubble = help_bubble_factory_registry_.CreateHelpBubble(
      element, std::move(params));

  EXPECT_TRUE(help_bubble);
  EXPECT_TRUE(help_bubble->is_open());

  EXPECT_CALL(
      test_handler_->mock(),
      HideHelpBubble(kHelpBubbleHandlerTestElementIdentifier.GetName()));
  EXPECT_TRUE(help_bubble->Close());

  EXPECT_FALSE(help_bubble->is_open());
}

TEST_F(HelpBubbleHandlerTest, FocusHelpBubble) {
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier.GetName(), true, kElementBounds);
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

  EXPECT_CALL(test_handler_->mock(),
              ToggleFocusForAccessibility(
                  kHelpBubbleHandlerTestElementIdentifier.GetName()));
  help_bubble_factory_registry_.ToggleFocusForAccessibility(
      test_handler_->context());
  EXPECT_CALL(test_handler_->mock(), ToggleFocusForAccessibility).Times(0);

  EXPECT_CALL(
      test_handler_->mock(),
      HideHelpBubble(kHelpBubbleHandlerTestElementIdentifier.GetName()));
  EXPECT_TRUE(help_bubble->Close());
}

TEST_F(HelpBubbleHandlerTest, ExternalHelpBubbleUpdated) {
  // Generate and retrieve a tracked element.
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier.GetName(), true, kElementBounds);
  auto* const element =
      ui::ElementTracker::GetElementTracker()->GetUniqueElement(
          kHelpBubbleHandlerTestElementIdentifier, test_handler_->context());

  // Create a dummy external help bubble.
  HelpBubbleParams params;
  params.body_text = u"Help bubble body.";
  params.close_button_alt_text = u"Close button alt text.";
  HelpBubbleButtonParams button_params;
  button_params.is_default = true;
  button_params.text = u"Button";
  params.buttons.emplace_back(std::move(button_params));

  std::unique_ptr<HelpBubble> help_bubble =
      std::make_unique<test::TestHelpBubble>(element, std::move(params));

  // Call the floating help bubble created method and ensure that the correct
  // message is sent over to the client.
  EXPECT_CALL(test_handler_->mock(),
              ExternalHelpBubbleUpdated(element->identifier().GetName(), true));
  test_handler_->OnFloatingHelpBubbleCreated(element->identifier(),
                                             help_bubble.get());

  // Close the bubble and verify that the correct message is sent, this that
  // there is no longer a bubble.
  EXPECT_CALL(
      test_handler_->mock(),
      ExternalHelpBubbleUpdated(element->identifier().GetName(), false));
  EXPECT_TRUE(help_bubble->Close());
}

TEST_F(HelpBubbleHandlerTest, HelpBubbleClosedWhenVisibilityChanges) {
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier.GetName(), true, kElementBounds);
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier2.GetName(), true, kElementBounds);
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
  EXPECT_TRUE(
      test_handler_->IsHelpBubbleShowingForTesting(element->identifier()));

  // This should have no effect since it's the wrong element.
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier2.GetName(), false, gfx::RectF());
  EXPECT_TRUE(help_bubble->is_open());

  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier.GetName(), false, gfx::RectF());
  EXPECT_FALSE(help_bubble->is_open());
  EXPECT_FALSE(
      test_handler_->IsHelpBubbleShowingForTesting(element->identifier()));
}

TEST_F(HelpBubbleHandlerTest, HelpBubbleClosedWhenClosedRemotely) {
  UNCALLED_MOCK_CALLBACK(HelpBubble::ClosedCallback, closed);

  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier.GetName(), true, kElementBounds);
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
  EXPECT_TRUE(
      test_handler_->IsHelpBubbleShowingForTesting(element->identifier()));

  EXPECT_CALL_IN_SCOPE(
      closed, Run,
      handler()->HelpBubbleClosed(
          kHelpBubbleHandlerTestElementIdentifier.GetName(),
          help_bubble::mojom::HelpBubbleClosedReason::kPageChanged));
  EXPECT_FALSE(help_bubble->is_open());
  EXPECT_FALSE(
      test_handler_->IsHelpBubbleShowingForTesting(element->identifier()));
}

TEST_F(HelpBubbleHandlerTest, DestroyHandlerCleansUpElement) {
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier.GetName(), true, kElementBounds);
  const ui::ElementContext context = test_handler_->context();
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kHelpBubbleHandlerTestElementIdentifier, context));
  test_handler_.reset();
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->IsElementVisible(
      kHelpBubbleHandlerTestElementIdentifier, context));
}

// Asserts that closing the HelpBubble handle to a bubble instance destroys
// the bubble.
TEST_F(HelpBubbleHandlerTest, DestroyBubbleWrapperClosesHelpBubble) {
  UNCALLED_MOCK_CALLBACK(HelpBubble::ClosedCallback, closed);

  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier.GetName(), true, kElementBounds);
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

  EXPECT_CALL(
      test_handler_->mock(),
      HideHelpBubble(kHelpBubbleHandlerTestElementIdentifier.GetName()));
  EXPECT_CALL_IN_SCOPE(closed, Run, help_bubble.reset());
}

TEST_F(HelpBubbleHandlerTest, HelpBubbleClosedWhenClosedByUserCallsDismiss) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, dismissed);

  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier.GetName(), true, kElementBounds);
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
  EXPECT_TRUE(
      test_handler_->IsHelpBubbleShowingForTesting(element->identifier()));

  EXPECT_CALL_IN_SCOPE(
      dismissed, Run,
      handler()->HelpBubbleClosed(
          kHelpBubbleHandlerTestElementIdentifier.GetName(),
          help_bubble::mojom::HelpBubbleClosedReason::kDismissedByUser));
  EXPECT_FALSE(help_bubble->is_open());
  EXPECT_FALSE(
      test_handler_->IsHelpBubbleShowingForTesting(element->identifier()));
}

TEST_F(HelpBubbleHandlerTest, ButtonPressedCallsCallback) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, button1_pressed);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, button2_pressed);

  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier.GetName(), true, kElementBounds);
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
  EXPECT_TRUE(
      test_handler_->IsHelpBubbleShowingForTesting(element->identifier()));

  EXPECT_CALL_IN_SCOPE(
      button2_pressed, Run,
      handler()->HelpBubbleButtonPressed(
          kHelpBubbleHandlerTestElementIdentifier.GetName(), 1));
  EXPECT_FALSE(help_bubble->is_open());
  EXPECT_FALSE(
      test_handler_->IsHelpBubbleShowingForTesting(element->identifier()));
}

TEST_F(HelpBubbleHandlerTest, ShowMultipleBubblesAndCloseOneViaVisibility) {
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier.GetName(), true, kElementBounds);
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier2.GetName(), true, kElementBounds);
  auto* const element =
      ui::ElementTracker::GetElementTracker()->GetUniqueElement(
          kHelpBubbleHandlerTestElementIdentifier, test_handler_->context());
  auto* const element2 =
      ui::ElementTracker::GetElementTracker()->GetUniqueElement(
          kHelpBubbleHandlerTestElementIdentifier2, test_handler_->context());
  ASSERT_NE(nullptr, element);
  ASSERT_NE(nullptr, element2);

  HelpBubbleParams params;
  params.body_text = u"Help bubble body.";
  params.arrow = HelpBubbleArrow::kTopCenter;

  EXPECT_CALL(test_handler_->mock(), ShowHelpBubble(testing::_));
  auto help_bubble = help_bubble_factory_registry_.CreateHelpBubble(
      element, std::move(params));
  EXPECT_TRUE(
      test_handler_->IsHelpBubbleShowingForTesting(element->identifier()));

  HelpBubbleParams params2;
  params2.body_text = u"Help bubble body 2.";
  params2.arrow = HelpBubbleArrow::kBottomLeft;
  EXPECT_CALL(test_handler_->mock(), ShowHelpBubble(testing::_));
  auto help_bubble2 = help_bubble_factory_registry_.CreateHelpBubble(
      element2, std::move(params2));
  EXPECT_TRUE(
      test_handler_->IsHelpBubbleShowingForTesting(element2->identifier()));

  EXPECT_TRUE(help_bubble->is_open());
  EXPECT_TRUE(help_bubble2->is_open());

  // Close one bubble without closing the other.
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier.GetName(), false, gfx::RectF());
  EXPECT_FALSE(help_bubble->is_open());
  EXPECT_TRUE(help_bubble2->is_open());
  EXPECT_FALSE(
      test_handler_->IsHelpBubbleShowingForTesting(element->identifier()));
  EXPECT_TRUE(
      test_handler_->IsHelpBubbleShowingForTesting(element2->identifier()));

  // When the second bubble goes away, it will attempt to close the bubble on
  // the remote.
  EXPECT_CALL(
      test_handler_->mock(),
      HideHelpBubble(kHelpBubbleHandlerTestElementIdentifier2.GetName()));
}

TEST_F(HelpBubbleHandlerTest, ShowMultipleBubblesAndCloseOneViaCallback) {
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier.GetName(), true, kElementBounds);
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier2.GetName(), true, kElementBounds);
  auto* const element =
      ui::ElementTracker::GetElementTracker()->GetUniqueElement(
          kHelpBubbleHandlerTestElementIdentifier, test_handler_->context());
  auto* const element2 =
      ui::ElementTracker::GetElementTracker()->GetUniqueElement(
          kHelpBubbleHandlerTestElementIdentifier2, test_handler_->context());
  ASSERT_NE(nullptr, element);
  ASSERT_NE(nullptr, element2);

  HelpBubbleParams params;
  params.body_text = u"Help bubble body.";
  params.arrow = HelpBubbleArrow::kTopCenter;

  EXPECT_CALL(test_handler_->mock(), ShowHelpBubble(testing::_));
  auto help_bubble = help_bubble_factory_registry_.CreateHelpBubble(
      element, std::move(params));
  EXPECT_TRUE(
      test_handler_->IsHelpBubbleShowingForTesting(element->identifier()));

  HelpBubbleParams params2;
  params2.body_text = u"Help bubble body 2.";
  params2.arrow = HelpBubbleArrow::kBottomLeft;
  EXPECT_CALL(test_handler_->mock(), ShowHelpBubble(testing::_));
  auto help_bubble2 = help_bubble_factory_registry_.CreateHelpBubble(
      element2, std::move(params2));
  EXPECT_TRUE(
      test_handler_->IsHelpBubbleShowingForTesting(element2->identifier()));

  EXPECT_TRUE(help_bubble->is_open());
  EXPECT_TRUE(help_bubble2->is_open());

  // Close one bubble without closing the other.
  handler()->HelpBubbleClosed(
      kHelpBubbleHandlerTestElementIdentifier.GetName(),
      help_bubble::mojom::HelpBubbleClosedReason::kPageChanged);
  EXPECT_FALSE(help_bubble->is_open());
  EXPECT_TRUE(help_bubble2->is_open());
  EXPECT_FALSE(
      test_handler_->IsHelpBubbleShowingForTesting(element->identifier()));
  EXPECT_TRUE(
      test_handler_->IsHelpBubbleShowingForTesting(element2->identifier()));

  // When the second bubble goes away, it will attempt to close the bubble on
  // the remote.
  EXPECT_CALL(
      test_handler_->mock(),
      HideHelpBubble(kHelpBubbleHandlerTestElementIdentifier2.GetName()));
}

// The following tests check that the WebContents visibility logic.

TEST_F(HelpBubbleHandlerTest, WebContentsNotVisibleResultsInNoElement) {
  UNCALLED_MOCK_CALLBACK(ui::ElementTracker::Callback, element_shown);
  const auto subscription =
      ui::ElementTracker::GetElementTracker()
          ->AddElementShownInAnyContextCallback(
              kHelpBubbleHandlerTestElementIdentifier, element_shown.Get());

  EXPECT_CALL(*visibility_provider_, CheckIsVisible)
      .WillOnce(testing::Return(false));
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier.GetName(), true, kElementBounds);
}

TEST_F(HelpBubbleHandlerTest, WebContentsVisibilityNotAvailable) {
  UNCALLED_MOCK_CALLBACK(ui::ElementTracker::Callback, element_shown);
  const auto subscription =
      ui::ElementTracker::GetElementTracker()
          ->AddElementShownInAnyContextCallback(
              kHelpBubbleHandlerTestElementIdentifier, element_shown.Get());

  EXPECT_CALL(*visibility_provider_, CheckIsVisible)
      .WillOnce(testing::Return(std::nullopt));
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier.GetName(), true, kElementBounds);
}

TEST_F(HelpBubbleHandlerTest, ElementShownOnmWebContentsBecomingVisible) {
  UNCALLED_MOCK_CALLBACK(ui::ElementTracker::Callback, element_shown);
  const auto subscription =
      ui::ElementTracker::GetElementTracker()
          ->AddElementShownInAnyContextCallback(
              kHelpBubbleHandlerTestElementIdentifier, element_shown.Get());

  EXPECT_CALL(*visibility_provider_, CheckIsVisible)
      .WillOnce(testing::Return(std::nullopt));
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier.GetName(), true, kElementBounds);

  EXPECT_CALL_IN_SCOPE(element_shown, Run,
                       visibility_provider_->SetLastKnownVisibility(true));
}

TEST_F(HelpBubbleHandlerTest, ElementHiddenWebContentsBecomingInvisible) {
  UNCALLED_MOCK_CALLBACK(ui::ElementTracker::Callback, element_hidden);

  ui::TrackedElement* el = nullptr;
  const auto sub1 = ui::ElementTracker::GetElementTracker()
                        ->AddElementShownInAnyContextCallback(
                            kHelpBubbleHandlerTestElementIdentifier,
                            base::BindLambdaForTesting(
                                [&el](ui::TrackedElement* el_) { el = el_; }));
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier.GetName(), true, kElementBounds);

  const auto sub2 =
      ui::ElementTracker::GetElementTracker()->AddElementHiddenCallback(
          el->identifier(), el->context(), element_hidden.Get());

  EXPECT_CALL_IN_SCOPE(element_hidden, Run,
                       visibility_provider_->SetLastKnownVisibility(false));
}

TEST_F(HelpBubbleHandlerTest, ElementHiddenWebContentsBecomingUnknown) {
  UNCALLED_MOCK_CALLBACK(ui::ElementTracker::Callback, element_shown);
  UNCALLED_MOCK_CALLBACK(ui::ElementTracker::Callback, element_hidden);

  const base::CallbackListSubscription sub1 =
      ui::ElementTracker::GetElementTracker()
          ->AddElementShownInAnyContextCallback(
              kHelpBubbleHandlerTestElementIdentifier, element_shown.Get());
  base::CallbackListSubscription sub2;

  EXPECT_CALL(element_shown, Run).WillOnce([&](ui::TrackedElement* el) {
    sub2 = ui::ElementTracker::GetElementTracker()->AddElementHiddenCallback(
        el->identifier(), el->context(), element_hidden.Get());
  });
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier.GetName(), true, kElementBounds);

  EXPECT_CALL_IN_SCOPE(
      element_hidden, Run,
      visibility_provider_->SetLastKnownVisibility(std::nullopt));
}

TEST_F(HelpBubbleHandlerTest, RepeatedlyQueriesVisibility) {
  UNCALLED_MOCK_CALLBACK(ui::ElementTracker::Callback, element_shown);
  const auto subscription =
      ui::ElementTracker::GetElementTracker()
          ->AddElementShownInAnyContextCallback(
              kHelpBubbleHandlerTestElementIdentifier, element_shown.Get());

  EXPECT_CALL(*visibility_provider_, CheckIsVisible)
      .Times(2)
      .WillRepeatedly(testing::Return(std::nullopt));
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier.GetName(), true, kElementBounds);
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier2.GetName(), true, kElementBounds);
}

TEST_F(HelpBubbleHandlerTest, WebContentsVisibilityCanChangeMultipleTimes) {
  UNCALLED_MOCK_CALLBACK(ui::ElementTracker::Callback, element_shown);
  const auto subscription =
      ui::ElementTracker::GetElementTracker()
          ->AddElementShownInAnyContextCallback(
              kHelpBubbleHandlerTestElementIdentifier, element_shown.Get());

  EXPECT_CALL_IN_SCOPE(element_shown, Run,
                       handler()->HelpBubbleAnchorVisibilityChanged(
                           kHelpBubbleHandlerTestElementIdentifier.GetName(),
                           true, kElementBounds));

  visibility_provider_->SetLastKnownVisibility(false);
  EXPECT_CALL_IN_SCOPE(element_shown, Run,
                       visibility_provider_->SetLastKnownVisibility(true));
  visibility_provider_->SetLastKnownVisibility(std::nullopt);
  EXPECT_CALL_IN_SCOPE(element_shown, Run,
                       visibility_provider_->SetLastKnownVisibility(true));
}

TEST_F(HelpBubbleHandlerTest, DestroyHandlerDuringCallback) {
  UNCALLED_MOCK_CALLBACK(ui::ElementTracker::Callback, element_shown);
  const auto subscription =
      ui::ElementTracker::GetElementTracker()
          ->AddElementShownInAnyContextCallback(
              kHelpBubbleHandlerTestElementIdentifier, element_shown.Get());

  EXPECT_CALL_IN_SCOPE(element_shown, Run,
                       handler()->HelpBubbleAnchorVisibilityChanged(
                           kHelpBubbleHandlerTestElementIdentifier.GetName(),
                           true, kElementBounds));
  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier2.GetName(), true, kElementBounds);

  visibility_provider_->SetLastKnownVisibility(false);
  EXPECT_CALL(element_shown, Run).WillOnce([&]() { test_handler_.reset(); });
  visibility_provider_->SetLastKnownVisibility(true);
}

TEST_F(HelpBubbleHandlerTest,
       WebUIHelpBubblePreventsHideWhenVisibilityChanges) {
  UNCALLED_MOCK_CALLBACK(ui::ElementTracker::Callback, element_hidden);

  handler()->HelpBubbleAnchorVisibilityChanged(
      kHelpBubbleHandlerTestElementIdentifier.GetName(), true, kElementBounds);
  auto* const element =
      ui::ElementTracker::GetElementTracker()->GetUniqueElement(
          kHelpBubbleHandlerTestElementIdentifier, test_handler_->context());
  ASSERT_NE(nullptr, element);
  const auto subscription =
      ui::ElementTracker::GetElementTracker()->AddElementHiddenCallback(
          element->identifier(), element->context(), element_hidden.Get());

  HelpBubbleParams params;
  params.body_text = u"Help bubble body.";
  params.arrow = HelpBubbleArrow::kTopCenter;

  EXPECT_CALL(test_handler_->mock(), ShowHelpBubble(testing::_));
  auto help_bubble = help_bubble_factory_registry_.CreateHelpBubble(
      element, std::move(params));

  visibility_provider_->SetLastKnownVisibility(false);
  EXPECT_TRUE(help_bubble->is_open());

  EXPECT_CALL(test_handler_->mock(), HideHelpBubble(testing::_));
  EXPECT_CALL_IN_SCOPE(element_hidden, Run, help_bubble->Close());
}

}  // namespace user_education
