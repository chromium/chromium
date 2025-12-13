// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/caption_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chromeos/ash/components/boca/babelorca/caption_bubble_settings_impl.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_caption_controller_delegate.h"
#include "chromeos/ash/components/boca/babelorca/testing_utils.h"
#include "components/live_caption/caption_bubble_context.h"
#include "components/live_caption/caption_bubble_controller.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/native_theme/caption_style.h"

namespace ash::babelorca {
namespace {

const std::string kApplicationLocale = "en-US";

void VerifyStyle(const ui::CaptionStyle& style,
                 const std::string& text_size = kCaptionsTextSize) {
  EXPECT_THAT(style.text_size, testing::HasSubstr(text_size));
  EXPECT_THAT(style.font_family, testing::HasSubstr(kCaptionsTextFont));
  EXPECT_THAT(style.text_color, testing::HasSubstr(kCaptionsTextColor));
  EXPECT_THAT(
      style.text_color,
      testing::HasSubstr(base::NumberToString(kCaptionsTextOpacity / 100.0)));
  EXPECT_THAT(style.background_color,
              testing::HasSubstr(kCaptionsBackgroundColor));
  EXPECT_THAT(style.background_color, testing::HasSubstr(base::NumberToString(
                                          kCaptionsBackgroundOpacity / 100.0)));
  EXPECT_THAT(style.text_shadow, testing::HasSubstr(kCaptionsTextShadow));
}

media::mojom::LanguageIdentificationEventPtr GetLanguageIdentificationEvent() {
  return media::mojom::LanguageIdentificationEvent::New(
      "ar-EG", media::mojom::ConfidenceLevel::kConfident,
      media::mojom::AsrSwitchResult::kSwitchSucceeded);
}

TEST(CaptionControllerTest, SetStyleOnStartLiveCaption) {
  TestingPrefServiceSimple pref_service;
  RegisterPrefsForTesting(&pref_service);
  auto delegate = std::make_unique<FakeCaptionControllerDelegate>();
  auto* delegate_ptr = delegate.get();
  CaptionController caption_controller(
      /*caption_bubble_context=*/nullptr, &pref_service, kApplicationLocale,
      /*caption_bubble_settings=*/nullptr, std::move(delegate));

  caption_controller.StartLiveCaption();

  ASSERT_EQ(delegate_ptr->GetUpdateCaptionStyleUpdates().size(), 1u);
  ASSERT_TRUE(delegate_ptr->GetUpdateCaptionStyleUpdates().at(0).has_value());
  VerifyStyle(delegate_ptr->GetUpdateCaptionStyleUpdates().at(0).value());
}

TEST(CaptionControllerTest, DispatchBeforeStartLiveCaption) {
  TestingPrefServiceSimple pref_service;
  RegisterPrefsForTesting(&pref_service);
  auto delegate = std::make_unique<FakeCaptionControllerDelegate>();
  auto* delegate_ptr = delegate.get();
  CaptionController caption_controller(
      /*caption_bubble_context=*/nullptr, &pref_service, kApplicationLocale,
      /*caption_bubble_settings=*/nullptr, std::move(delegate));

  bool dispatch_success = caption_controller.DispatchTranscription(
      media::SpeechRecognitionResult("transcript", /*is_final=*/true));

  EXPECT_FALSE(dispatch_success);
  EXPECT_EQ(delegate_ptr->GetCreateBubbleControllerCount(), 0u);
  EXPECT_THAT(delegate_ptr->GetCaptionStyleObserver(), testing::IsNull());
}

TEST(CaptionControllerTest, DispatchTranscription) {
  TestingPrefServiceSimple pref_service;
  RegisterPrefsForTesting(&pref_service);
  auto delegate = std::make_unique<FakeCaptionControllerDelegate>();
  auto* delegate_ptr = delegate.get();
  CaptionController caption_controller(
      /*caption_bubble_context=*/nullptr, &pref_service, kApplicationLocale,
      /*caption_bubble_settings=*/nullptr, std::move(delegate));

  caption_controller.StartLiveCaption();
  media::SpeechRecognitionResult transcript("transcript", /*is_final=*/true);
  bool success = caption_controller.DispatchTranscription(transcript);

  EXPECT_TRUE(success);
  EXPECT_EQ(delegate_ptr->GetCreateBubbleControllerCount(), 1u);
  ASSERT_EQ(delegate_ptr->GetTranscriptions().size(), 1u);
  EXPECT_EQ(delegate_ptr->GetTranscriptions().at(0), transcript);
}

TEST(CaptionControllerTest, DispatchAfterStopLiveCaption) {
  TestingPrefServiceSimple pref_service;
  RegisterPrefsForTesting(&pref_service);
  auto delegate = std::make_unique<FakeCaptionControllerDelegate>();
  auto* delegate_ptr = delegate.get();
  CaptionController caption_controller(
      /*caption_bubble_context=*/nullptr, &pref_service, kApplicationLocale,
      /*caption_bubble_settings=*/nullptr, std::move(delegate));

  caption_controller.StartLiveCaption();
  caption_controller.StopLiveCaption();
  bool dispatch_success = caption_controller.DispatchTranscription(
      media::SpeechRecognitionResult("transcript", /*is_final=*/true));

  EXPECT_FALSE(dispatch_success);
  EXPECT_EQ(delegate_ptr->GetCreateBubbleControllerCount(), 1u);
}

TEST(CaptionControllerTest, OnLanguageIdentificationEventBeforeStart) {
  TestingPrefServiceSimple pref_service;
  RegisterPrefsForTesting(&pref_service);
  auto delegate = std::make_unique<FakeCaptionControllerDelegate>();
  auto* delegate_ptr = delegate.get();
  CaptionController caption_controller(
      /*caption_bubble_context=*/nullptr, &pref_service, kApplicationLocale,
      /*caption_bubble_settings=*/nullptr, std::move(delegate));

  caption_controller.OnLanguageIdentificationEvent(
      GetLanguageIdentificationEvent());

  EXPECT_EQ(delegate_ptr->GetOnLanguageIdentificationEventCount(), 0u);
}

TEST(CaptionControllerTest, OnLanguageIdentificationEvent) {
  TestingPrefServiceSimple pref_service;
  RegisterPrefsForTesting(&pref_service);
  auto delegate = std::make_unique<FakeCaptionControllerDelegate>();
  auto* delegate_ptr = delegate.get();
  CaptionController caption_controller(
      /*caption_bubble_context=*/nullptr, &pref_service, kApplicationLocale,
      /*caption_bubble_settings=*/nullptr, std::move(delegate));

  caption_controller.StartLiveCaption();
  caption_controller.OnLanguageIdentificationEvent(
      GetLanguageIdentificationEvent());

  EXPECT_EQ(delegate_ptr->GetOnLanguageIdentificationEventCount(), 1u);
}

TEST(CaptionControllerTest, OnLanguageIdentificationEventAfterStop) {
  TestingPrefServiceSimple pref_service;
  RegisterPrefsForTesting(&pref_service);
  auto delegate = std::make_unique<FakeCaptionControllerDelegate>();
  auto* delegate_ptr = delegate.get();
  CaptionController caption_controller(
      /*caption_bubble_context=*/nullptr, &pref_service, kApplicationLocale,
      /*caption_bubble_settings=*/nullptr, std::move(delegate));

  caption_controller.StartLiveCaption();
  caption_controller.StopLiveCaption();
  caption_controller.OnLanguageIdentificationEvent(
      GetLanguageIdentificationEvent());

  EXPECT_EQ(delegate_ptr->GetOnLanguageIdentificationEventCount(), 0u);
}

TEST(CaptionControllerTest, OnCaptionStyleUpdated) {
  TestingPrefServiceSimple pref_service;
  RegisterPrefsForTesting(&pref_service);
  auto delegate = std::make_unique<FakeCaptionControllerDelegate>();
  auto* delegate_ptr = delegate.get();
  CaptionController caption_controller(
      /*caption_bubble_context=*/nullptr, &pref_service, kApplicationLocale,
      /*caption_bubble_settings=*/nullptr, std::move(delegate));

  caption_controller.StartLiveCaption();
  ASSERT_THAT(delegate_ptr->GetCaptionStyleObserver(), testing::NotNull());
  delegate_ptr->GetCaptionStyleObserver()->OnCaptionStyleUpdated();

  ASSERT_EQ(delegate_ptr->GetUpdateCaptionStyleUpdates().size(), 2u);
  ASSERT_TRUE(delegate_ptr->GetUpdateCaptionStyleUpdates().at(1).has_value());
  VerifyStyle(delegate_ptr->GetUpdateCaptionStyleUpdates().at(1).value());
}

TEST(CaptionControllerTest, OnCaptionStylePrefChange) {
  const std::string kNewCaptionsTextSize = "50%";
  TestingPrefServiceSimple pref_service;
  RegisterPrefsForTesting(&pref_service);
  auto delegate = std::make_unique<FakeCaptionControllerDelegate>();
  auto* delegate_ptr = delegate.get();
  CaptionController caption_controller(
      /*caption_bubble_context=*/nullptr, &pref_service, kApplicationLocale,
      /*caption_bubble_settings=*/nullptr, std::move(delegate));

  caption_controller.StartLiveCaption();
  pref_service.SetUserPref(prefs::kAccessibilityCaptionsTextSize,
                           base::Value(kNewCaptionsTextSize));

  ASSERT_EQ(delegate_ptr->GetUpdateCaptionStyleUpdates().size(), 2u);
  ASSERT_TRUE(delegate_ptr->GetUpdateCaptionStyleUpdates().at(1).has_value());
  VerifyStyle(delegate_ptr->GetUpdateCaptionStyleUpdates().at(1).value(),
              kNewCaptionsTextSize);
}

TEST(CaptionControllerTest, NoCaptionStyleUpdatesAfterStopLiveCaption) {
  TestingPrefServiceSimple pref_service;
  RegisterPrefsForTesting(&pref_service);
  auto delegate = std::make_unique<FakeCaptionControllerDelegate>();
  auto* delegate_ptr = delegate.get();
  CaptionController caption_controller(
      /*caption_bubble_context=*/nullptr, &pref_service, kApplicationLocale,
      /*caption_bubble_settings=*/nullptr, std::move(delegate));

  caption_controller.StartLiveCaption();
  caption_controller.StopLiveCaption();
  pref_service.SetUserPref(prefs::kAccessibilityCaptionsTextSize,
                           base::Value("10%"));

  EXPECT_THAT(delegate_ptr->GetCaptionStyleObserver(), testing::IsNull());
}

TEST(CaptionControllerTest, DispatchTranscriptionFailed) {
  TestingPrefServiceSimple pref_service;
  RegisterPrefsForTesting(&pref_service);
  auto delegate = std::make_unique<FakeCaptionControllerDelegate>();
  auto* delegate_ptr = delegate.get();
  CaptionController caption_controller(
      /*caption_bubble_context=*/nullptr, &pref_service, kApplicationLocale,
      /*caption_bubble_settings=*/nullptr, std::move(delegate));

  caption_controller.StartLiveCaption();
  delegate_ptr->SetOnTranscriptionSuccess(/*success=*/false);
  media::SpeechRecognitionResult transcript("transcript", /*is_final=*/true);
  bool success = caption_controller.DispatchTranscription(transcript);

  EXPECT_FALSE(success);
  EXPECT_EQ(delegate_ptr->GetCreateBubbleControllerCount(), 1u);
  ASSERT_EQ(delegate_ptr->GetTranscriptions().size(), 1u);
  EXPECT_EQ(delegate_ptr->GetTranscriptions().at(0), transcript);
}

using CaptionControllerTranslateTest =
    testing::TestWithParam<std::tuple<bool, bool>>;

TEST_P(CaptionControllerTranslateTest, IsTranslateAllowedAndEnabled) {
  TestingPrefServiceSimple pref_service;
  RegisterPrefsForTesting(&pref_service);
  bool allowed = std::get<0>(GetParam());
  bool enabled = std::get<1>(GetParam());
  CaptionController caption_controller(
      /*caption_bubble_context=*/nullptr, &pref_service, kApplicationLocale,
      std::make_unique<CaptionBubbleSettingsImpl>(
          &pref_service, kApplicationLocale, base::DoNothing()),
      std::make_unique<FakeCaptionControllerDelegate>());

  caption_controller.SetLiveTranslateEnabled(enabled);
  caption_controller.SetTranslateAllowed(allowed);

  EXPECT_EQ(caption_controller.IsTranslateAllowedAndEnabled(),
            allowed && enabled);
}

INSTANTIATE_TEST_SUITE_P(CaptionControllerTranslateTestSuite,
                         CaptionControllerTranslateTest,
                         testing::Combine(testing::Bool(), testing::Bool()));
}  // namespace
}  // namespace ash::babelorca
