// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/libassistant/conversation_controller.h"
#include "chromeos/ash/services/libassistant/libassistant_service.h"
#include "chromeos/ash/services/libassistant/public/cpp/android_app_info.h"
#include "chromeos/ash/services/libassistant/public/mojom/conversation_observer.mojom.h"
#include "chromeos/ash/services/libassistant/test_support/libassistant_service_tester.h"
#include "chromeos/assistant/internal/action/cros_action_module.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::libassistant {

namespace {

// Helper class to fire interaction response handlers for tests.
class CrosActionModuleHelper {
 public:
  explicit CrosActionModuleHelper(
      chromeos::assistant::action::CrosActionModule* action_module)
      : action_module_(*action_module) {}
  CrosActionModuleHelper(const CrosActionModuleHelper&) = delete;
  CrosActionModuleHelper& operator=(const CrosActionModuleHelper&) = delete;
  ~CrosActionModuleHelper() = default;

  void ShowHtml(const std::string& html) {
    for (auto* observer : action_observers())
      observer->OnShowHtml(html, /*fallback=*/"");
  }

  void ShowText(const std::string& text) {
    for (auto* observer : action_observers())
      observer->OnShowText(text);
  }

  void ShowSuggestions(
      const std::vector<chromeos::assistant::action::Suggestion>& suggestions) {
    for (auto* observer : action_observers())
      observer->OnShowSuggestions(suggestions);
  }

  void OpenUrl(const std::string& url, bool in_background) {
    for (auto* observer : action_observers())
      observer->OnOpenUrl(url, in_background);
  }

  void OpenAndroidApp(const assistant::AndroidAppInfo& app_info) {
    chromeos::assistant::InteractionInfo info{};
    for (auto* observer : action_observers())
      observer->OnOpenAndroidApp(app_info, info);
  }

  void ScheduleWait() {
    for (auto* observer : action_observers())
      observer->OnScheduleWait(/*id=*/012, /*time_ms=*/123);
  }

 private:
  const std::vector<chromeos::assistant::action::AssistantActionObserver*>&
  action_observers() {
    return action_module_->GetActionObserversForTesting();
  }

  const raw_ref<const chromeos::assistant::action::CrosActionModule>
      action_module_;
};

class ConversationObserverMock : public mojom::ConversationObserver {
 public:
  ConversationObserverMock() = default;
  ConversationObserverMock(const ConversationObserverMock&) = delete;
  ConversationObserverMock& operator=(const ConversationObserverMock&) = delete;
  ~ConversationObserverMock() override = default;

  // mojom::ConversationObserver implementation:
  MOCK_METHOD(void,
              OnInteractionStarted,
              (const ::ash::assistant::AssistantInteractionMetadata& metadata));
  MOCK_METHOD(void,
              OnInteractionFinished,
              (assistant::AssistantInteractionResolution resolution));
  MOCK_METHOD(void, OnTtsStarted, (bool due_to_error));
  MOCK_METHOD(void,
              OnHtmlResponse,
              (const std::string& response, const std::string& fallback));
  MOCK_METHOD(void, OnTextResponse, (const std::string& text));
  MOCK_METHOD(void,
              OnSuggestionsResponse,
              (const std::vector<assistant::AssistantSuggestion>& suggestions));
  MOCK_METHOD(void, OnOpenUrlResponse, (const GURL& url, bool in_background));
  MOCK_METHOD(void,
              OnOpenAppResponse,
              (const assistant::AndroidAppInfo& app_info));
  MOCK_METHOD(void, OnWaitStarted, ());

  mojo::PendingRemote<mojom::ConversationObserver> BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

 private:
  mojo::Receiver<mojom::ConversationObserver> receiver_{this};
};

}  // namespace

class AssistantConversationObserverTest : public ::testing::Test {
 public:
  AssistantConversationObserverTest() = default;
  AssistantConversationObserverTest(const AssistantConversationObserverTest&) =
      delete;
  AssistantConversationObserverTest& operator=(
      const AssistantConversationObserverTest&) = delete;
  ~AssistantConversationObserverTest() override = default;

  void SetUp() override {
    service_tester_.conversation_controller().AddRemoteObserver(
        observer_mock_.BindNewPipeAndPassRemote());

    service_tester_.Start();

    controller().OnAssistantClientRunning(&service_tester_.assistant_client());

    action_module_helper_ = std::make_unique<CrosActionModuleHelper>(
        static_cast<chromeos::assistant::action::CrosActionModule*>(
            controller().action_module()));
  }

  assistant_client::ConversationStateListener& conversation_state_listener() {
    return *service_tester_.assistant_manager().conversation_state_listener();
  }

  CrosActionModuleHelper& action_module_helper() {
    return *action_module_helper_.get();
  }

  ConversationObserverMock& observer_mock() { return observer_mock_; }

  ConversationController& controller() {
    return service_tester_.service().conversation_controller();
  }

 private:
  base::test::SingleThreadTaskEnvironment environment_;
  ::testing::StrictMock<ConversationObserverMock> observer_mock_;
  LibassistantServiceTester service_tester_;
  std::unique_ptr<CrosActionModuleHelper> action_module_helper_;
};

TEST_F(AssistantConversationObserverTest,
       ShouldReceiveOnTurnFinishedEventWhenFinishedNormally) {
  EXPECT_CALL(observer_mock(),
              OnInteractionFinished(
                  assistant::AssistantInteractionResolution::kNormal));

  conversation_state_listener().OnConversationTurnFinished(
      assistant_client::ConversationStateListener::Resolution::NORMAL);
  observer_mock().FlushForTesting();
}

TEST_F(AssistantConversationObserverTest,
       ShouldReceiveOnTurnFinishedEventWhenBeingInterrupted) {
  EXPECT_CALL(observer_mock(),
              OnInteractionFinished(
                  assistant::AssistantInteractionResolution::kInterruption));

  conversation_state_listener().OnConversationTurnFinished(
      assistant_client::ConversationStateListener::Resolution::BARGE_IN);
  observer_mock().FlushForTesting();
}

TEST_F(AssistantConversationObserverTest,
       ShouldReceiveOnTtsStartedEventWhenFinishingNormally) {
  EXPECT_CALL(observer_mock(), OnTtsStarted(/*due_to_error=*/false));

  conversation_state_listener().OnRespondingStarted(false);
  observer_mock().FlushForTesting();
}

TEST_F(AssistantConversationObserverTest,
       ShouldReceiveOnTtsStartedEventWhenErrorOccured) {
  EXPECT_CALL(observer_mock(), OnTtsStarted(/*due_to_error=*/true));

  conversation_state_listener().OnRespondingStarted(true);
  observer_mock().FlushForTesting();
}

TEST_F(AssistantConversationObserverTest, ShouldReceiveOnHtmlResponse) {
  const std::string fake_html = "<h1>Hello world!</h1>";
  EXPECT_CALL(observer_mock(), OnHtmlResponse(fake_html, ""));

  // Fallback is always empty since it has been deprecated.
  action_module_helper().ShowHtml(/*html=*/fake_html);
  observer_mock().FlushForTesting();
}

TEST_F(AssistantConversationObserverTest, ShouldReceiveOnTextResponse) {
  const std::string fake_text = "I'm a text response";
  EXPECT_CALL(observer_mock(), OnTextResponse(fake_text));

  action_module_helper().ShowText(fake_text);
  observer_mock().FlushForTesting();
}

TEST_F(AssistantConversationObserverTest, ShouldReceiveOnSuggestionsResponse) {
  const std::string fake_text = "text";
  const std::string fake_icon_url = "https://icon-url/";
  const std::string fake_action_url = "https://action-url/";
  std::vector<chromeos::assistant::action::Suggestion> fake_suggestions{
      {fake_text, fake_icon_url, fake_action_url}};

  EXPECT_CALL(observer_mock(), OnSuggestionsResponse)
      .WillOnce(testing::Invoke(
          [&](const std::vector<assistant::AssistantSuggestion>& suggestions) {
            EXPECT_EQ(fake_text, suggestions[0].text);
            EXPECT_EQ(GURL(fake_icon_url), suggestions[0].icon_url);
            EXPECT_EQ(GURL(fake_action_url), suggestions[0].action_url);
          }));

  action_module_helper().ShowSuggestions(fake_suggestions);
  observer_mock().FlushForTesting();
}

TEST_F(AssistantConversationObserverTest, ShouldReceiveOnOpenUrlResponse) {
  const std::string fake_url = "https://fake-url/";
  EXPECT_CALL(observer_mock(),
              OnOpenUrlResponse(GURL(fake_url), /*in_background=*/false));

  action_module_helper().OpenUrl(fake_url, /*in_background=*/false);
  observer_mock().FlushForTesting();
}

TEST_F(AssistantConversationObserverTest, ShouldReceiveOnOpenAppResponse) {
  assistant::AndroidAppInfo fake_app_info;
  fake_app_info.package_name = "fake package name";
  fake_app_info.version = 123;
  fake_app_info.localized_app_name = "fake localized name";
  fake_app_info.action = "fake action";
  fake_app_info.intent = "fake intent";
  fake_app_info.status = assistant::AppStatus::kUnknown;

  EXPECT_CALL(observer_mock(), OnOpenAppResponse)
      .WillOnce(testing::Invoke([&](const assistant::AndroidAppInfo& app_info) {
        EXPECT_EQ("fake package name", app_info.package_name);
        EXPECT_EQ(123, app_info.version);
        EXPECT_EQ("fake localized name", app_info.localized_app_name);
        EXPECT_EQ("fake action", app_info.action);
        EXPECT_EQ(assistant::AppStatus::kUnknown, app_info.status);
      }));

  action_module_helper().OpenAndroidApp(fake_app_info);
  observer_mock().FlushForTesting();
}

TEST_F(AssistantConversationObserverTest, ShouldReceiveOnWaitStarted) {
  EXPECT_CALL(observer_mock(), OnWaitStarted());

  action_module_helper().ScheduleWait();
  observer_mock().FlushForTesting();
}

}  // namespace ash::libassistant
