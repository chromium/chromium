// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/caption_controller_base.h"

#include <memory>
#include <string>

#include "components/live_caption/caption_bubble_context.h"
#include "components/live_caption/caption_bubble_controller.h"
#include "components/live_caption/pref_names.h"
#include "components/live_caption/views/translation_view_wrapper_base.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace captions {
namespace {

// CaptionControllerBase that we'll instantiate for test.
class TestCaptionControllerBase : public CaptionControllerBase {
 public:
  TestCaptionControllerBase(PrefService* profile_prefs,
                            const std::string& application_locale,
                            std::unique_ptr<Delegate> delegate)
      : CaptionControllerBase(profile_prefs,
                              application_locale,
                              std::move(delegate)) {}

  MOCK_METHOD(CaptionBubbleSettings*, caption_bubble_settings, (), (override));
};

class MockListener : public CaptionControllerBase::Listener {
 public:
  explicit MockListener(bool* free_flag = nullptr) : free_flag_(free_flag) {}
  ~MockListener() override {
    if (free_flag_) {
      *free_flag_ = true;
    }
  }

  MOCK_METHOD(bool,
              OnTranscription,
              (content::WebContents*,
               CaptionBubbleContext*,
               const media::SpeechRecognitionResult&),
              (override));
  MOCK_METHOD(void,
              OnAudioStreamEnd,
              (content::WebContents*, CaptionBubbleContext*),
              (override));
  MOCK_METHOD(void,
              OnLanguageIdentificationEvent,
              (content::WebContents*,
               CaptionBubbleContext*,
               const media::mojom::LanguageIdentificationEventPtr&),
              (override));

 private:
  raw_ptr<bool> free_flag_ = nullptr;
};

class MockCaptionBubbleContext : public CaptionBubbleContext {
 public:
  ~MockCaptionBubbleContext() override = default;

  MOCK_METHOD(void, GetBounds, (GetBoundsCallback), (const override));
  // MOCK_METHOD hates "const std::string" as a return type.
  const std::string GetSessionId() const override { return {}; }
  MOCK_METHOD(void, Activate, (), (override));
  MOCK_METHOD(bool, IsActivatable, (), (const override));
  MOCK_METHOD(bool, ShouldAvoidOverlap, (), (const override));
  MOCK_METHOD(std::unique_ptr<CaptionBubbleSessionObserver>,
              GetCaptionBubbleSessionObserver,
              (),
              (override));
  MOCK_METHOD(OpenCaptionSettingsCallback,
              GetOpenCaptionSettingsCallback,
              (),
              (override));
  MOCK_METHOD(void,
              SetContextActivatabilityObserver,
              (CaptionBubble*),
              (override));
  MOCK_METHOD(void, RemoveContextActivatabilityObserver, (), (override));
};

class MockCaptionBubbleController : public CaptionBubbleController {
 public:
  MockCaptionBubbleController() = default;
  ~MockCaptionBubbleController() override = default;

  // CaptionControllerBase::Listener
  MOCK_METHOD(bool,
              OnTranscription,
              (content::WebContents*,
               CaptionBubbleContext*,
               const media::SpeechRecognitionResult&),
              (override));
  MOCK_METHOD(void,
              OnAudioStreamEnd,
              (content::WebContents*, CaptionBubbleContext*),
              (override));
  MOCK_METHOD(void,
              OnLanguageIdentificationEvent,
              (content::WebContents*,
               CaptionBubbleContext*,
               const media::mojom::LanguageIdentificationEventPtr&),
              (override));

  // CaptionBubbleController
  MOCK_METHOD(void,
              OnError,
              (CaptionBubbleContext*,
               CaptionBubbleErrorType,
               OnErrorClickedCallback,
               OnDoNotShowAgainClickedCallback),
              (override));
  MOCK_METHOD(void,
              UpdateCaptionStyle,
              (std::optional<ui::CaptionStyle>),
              (override));
  MOCK_METHOD(bool, IsWidgetVisibleForTesting, (), (override));
  MOCK_METHOD(bool, IsGenericErrorMessageVisibleForTesting, (), (override));
  MOCK_METHOD(std::string, GetBubbleLabelTextForTesting, (), (override));
  MOCK_METHOD(void, CloseActiveModelForTesting, (), (override));
};

class MockCaptionControllerDelegate : public CaptionControllerBase::Delegate {
 public:
  explicit MockCaptionControllerDelegate(
      std::unique_ptr<CaptionBubbleController> bubble_controller) {
    EXPECT_CALL(*this, CreateCaptionBubbleController(_, _, _))
        .WillOnce(Return(std::move(bubble_controller)));
  }
  ~MockCaptionControllerDelegate() override = default;

  MOCK_METHOD(std::unique_ptr<CaptionBubbleController>,
              CreateCaptionBubbleController,
              (CaptionBubbleSettings*,
               const std::string&,
               std::unique_ptr<TranslationViewWrapperBase>),
              (override));

  void AddCaptionStyleObserver(ui::NativeThemeObserver*) override {}

  void RemoveCaptionStyleObserver(ui::NativeThemeObserver*) override {}
};

void RegisterStylePrefs(TestingPrefServiceSimple* pref_service) {
  const std::string kCaptionsTextSize = "20%";
  const std::string kCaptionsTextFont = "aerial";
  const std::string kCaptionsTextColor = "255,99,71";
  const std::string kCaptionsBackgroundColor = "90,255,50";
  const std::string kCaptionsTextShadow = "10px";
  constexpr int kCaptionsTextOpacity = 50;
  constexpr int kCaptionsBackgroundOpacity = 30;

  pref_service->registry()->RegisterStringPref(
      prefs::kAccessibilityCaptionsTextSize, kCaptionsTextSize);
  pref_service->registry()->RegisterStringPref(
      prefs::kAccessibilityCaptionsTextFont, kCaptionsTextFont);
  pref_service->registry()->RegisterStringPref(
      prefs::kAccessibilityCaptionsTextColor, kCaptionsTextColor);
  pref_service->registry()->RegisterIntegerPref(
      prefs::kAccessibilityCaptionsTextOpacity, kCaptionsTextOpacity);
  pref_service->registry()->RegisterStringPref(
      prefs::kAccessibilityCaptionsBackgroundColor, kCaptionsBackgroundColor);
  pref_service->registry()->RegisterStringPref(
      prefs::kAccessibilityCaptionsTextShadow, kCaptionsTextShadow);
  pref_service->registry()->RegisterIntegerPref(
      prefs::kAccessibilityCaptionsBackgroundOpacity,
      kCaptionsBackgroundOpacity);
}

class CaptionControllerBaseTest : public testing::Test {
 public:
  ~CaptionControllerBaseTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();

    RegisterStylePrefs(&testing_pref_service_);
  }

  std::unique_ptr<TestCaptionControllerBase> CreateController(
      std::unique_ptr<CaptionControllerBase::Delegate> delegate = nullptr) {
    return std::make_unique<TestCaptionControllerBase>(
        &testing_pref_service_, speech::kUsEnglishLocale, std::move(delegate));
  }

  TestingPrefServiceSimple testing_pref_service_;
};

TEST_F(CaptionControllerBaseTest, ListenersAreFreedOnDestruction) {
  bool was_freed = false;
  auto listener = std::make_unique<MockListener>(&was_freed);

  auto controller_under_test = CreateController();
  controller_under_test->AddListener(std::move(listener));
  EXPECT_FALSE(was_freed);
  controller_under_test.reset();
  EXPECT_TRUE(was_freed);
}

TEST_F(CaptionControllerBaseTest, CaptionBubbleControllerReceivesCallbacks) {
  auto mock_bubble_controller = std::make_unique<MockCaptionBubbleController>();
  auto* mock_bubble_controller_raw = mock_bubble_controller.get();
  auto controller_under_test =
      CreateController(std::make_unique<MockCaptionControllerDelegate>(
          std::move(mock_bubble_controller)));
  controller_under_test->create_ui_for_testing();
  EXPECT_CALL(*mock_bubble_controller_raw, OnAudioStreamEnd(nullptr, nullptr));
  controller_under_test->OnAudioStreamEnd(nullptr, nullptr);
}

TEST_F(CaptionControllerBaseTest, CaptionBubbleAliasIsAddedAndRemoved) {
  auto mock_bubble_controller = std::make_unique<MockCaptionBubbleController>();
  auto* mock_bubble_controller_raw = mock_bubble_controller.get();
  auto controller_under_test =
      CreateController(std::make_unique<MockCaptionControllerDelegate>(
          std::move(mock_bubble_controller)));

  // Create the UI, and expect that its alias is now correct.
  controller_under_test->create_ui_for_testing();
  EXPECT_EQ(mock_bubble_controller_raw,
            controller_under_test->caption_bubble_controller_for_testing());

  // Delete the UI, and expect that its alias is now cleared.
  controller_under_test->destroy_ui_for_testing();
  EXPECT_EQ(nullptr,
            controller_under_test->caption_bubble_controller_for_testing());
}

TEST_F(CaptionControllerBaseTest, ListenersReceiveTranscription) {
  MockCaptionBubbleContext context;

  auto listener = std::make_unique<MockListener>();
  content::WebContents* web_contents =
      reinterpret_cast<content::WebContents*>(listener.get());
  EXPECT_CALL(*listener, OnAudioStreamEnd(web_contents, &context));

  auto controller_under_test = CreateController();
  controller_under_test->AddListener(std::move(listener));
  controller_under_test->OnAudioStreamEnd(web_contents, &context);
}

TEST_F(CaptionControllerBaseTest, TranscriptionStopsIfNoListeners) {
  MockCaptionBubbleContext context;
  media::SpeechRecognitionResult result;

  auto controller_under_test = CreateController();
  EXPECT_FALSE(controller_under_test->DispatchTranscription(
      /*web_contents=*/nullptr, &context, result));
}

TEST_F(CaptionControllerBaseTest, ListenersReceiveAudioEnd) {
  MockCaptionBubbleContext context;
  media::SpeechRecognitionResult result;

  auto listener = std::make_unique<MockListener>();
  content::WebContents* web_contents =
      reinterpret_cast<content::WebContents*>(listener.get());
  EXPECT_CALL(*listener, OnTranscription(web_contents, &context, result));

  auto controller_under_test = CreateController();
  controller_under_test->AddListener(std::move(listener));
  controller_under_test->DispatchTranscription(web_contents, &context, result);
}

}  // namespace
}  // namespace captions
