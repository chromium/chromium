// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/caption_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/values.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_caption_controller_delegate.h"
#include "components/live_caption/caption_bubble_context.h"
#include "components/live_caption/caption_bubble_controller.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/native_theme/caption_style.h"

namespace ash::babelorca {
namespace {

const std::string kApplicationLocale = "en-US";

const std::string kCaptionsTextSize = "20%";
const std::string kCaptionsTextFont = "aerial";
const std::string kCaptionsTextColor = "255,99,71";
const std::string kCaptionsBackgroundColor = "90,255,50";
const std::string kCaptionsTextShadow = "10px";

constexpr int kCaptionsTextOpacity = 50;
constexpr int kCaptionsBackgroundOpacity = 30;

void RegisterPrefs(TestingPrefServiceSimple* pref_service) {
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

TEST(CaptionControllerTest, SetStyleOnStartLiveCaption) {
  TestingPrefServiceSimple pref_service;
  RegisterPrefs(&pref_service);
  auto delegate = std::make_unique<FakeCaptionControllerDelegate>();
  auto* delegate_ptr = delegate.get();
  CaptionController caption_controller(
      /*caption_bubble_context=*/nullptr, &pref_service, kApplicationLocale,
      std::move(delegate));

  caption_controller.StartLiveCaption();

  ASSERT_EQ(delegate_ptr->GetUpdateCaptionStyleUpdates().size(), 1u);
  ASSERT_TRUE(delegate_ptr->GetUpdateCaptionStyleUpdates().at(0).has_value());
  VerifyStyle(delegate_ptr->GetUpdateCaptionStyleUpdates().at(0).value());
}

TEST(CaptionControllerTest, DispatchBeforeStartLiveCaption) {
  TestingPrefServiceSimple pref_service;
  RegisterPrefs(&pref_service);
  auto delegate = std::make_unique<FakeCaptionControllerDelegate>();
  auto* delegate_ptr = delegate.get();
  CaptionController caption_controller(
      /*caption_bubble_context=*/nullptr, &pref_service, kApplicationLocale,
      std::move(delegate));

  bool dispatch_success = caption_controller.DispatchTranscription(
      media::SpeechRecognitionResult("transcript", /*is_final=*/true));

  EXPECT_FALSE(dispatch_success);
  EXPECT_EQ(delegate_ptr->GetCreateBubbleControllerCount(), 0u);
  EXPECT_THAT(delegate_ptr->GetCaptionStyleObserver(), testing::IsNull());
}

TEST(CaptionControllerTest, DispatchTranscription) {
  TestingPrefServiceSimple pref_service;
  RegisterPrefs(&pref_service);
  auto delegate = std::make_unique<FakeCaptionControllerDelegate>();
  auto* delegate_ptr = delegate.get();
  CaptionController caption_controller(
      /*caption_bubble_context=*/nullptr, &pref_service, kApplicationLocale,
      std::move(delegate));

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
  RegisterPrefs(&pref_service);
  auto delegate = std::make_unique<FakeCaptionControllerDelegate>();
  auto* delegate_ptr = delegate.get();
  CaptionController caption_controller(
      /*caption_bubble_context=*/nullptr, &pref_service, kApplicationLocale,
      std::move(delegate));

  caption_controller.StartLiveCaption();
  caption_controller.StopLiveCaption();
  bool dispatch_success = caption_controller.DispatchTranscription(
      media::SpeechRecognitionResult("transcript", /*is_final=*/true));

  EXPECT_FALSE(dispatch_success);
  EXPECT_EQ(delegate_ptr->GetCreateBubbleControllerCount(), 1u);
}

TEST(CaptionControllerTest, OnAudioStreamEndBeforeStart) {
  TestingPrefServiceSimple pref_service;
  RegisterPrefs(&pref_service);
  auto delegate = std::make_unique<FakeCaptionControllerDelegate>();
  auto* delegate_ptr = delegate.get();
  CaptionController caption_controller(
      /*caption_bubble_context=*/nullptr, &pref_service, kApplicationLocale,
      std::move(delegate));

  caption_controller.OnAudioStreamEnd();

  EXPECT_EQ(delegate_ptr->GetOnAudioStreamEndCount(), 0u);
}

TEST(CaptionControllerTest, OnAudioStreamEnd) {
  TestingPrefServiceSimple pref_service;
  RegisterPrefs(&pref_service);
  auto delegate = std::make_unique<FakeCaptionControllerDelegate>();
  auto* delegate_ptr = delegate.get();
  CaptionController caption_controller(
      /*caption_bubble_context=*/nullptr, &pref_service, kApplicationLocale,
      std::move(delegate));

  caption_controller.StartLiveCaption();
  caption_controller.OnAudioStreamEnd();

  EXPECT_EQ(delegate_ptr->GetOnAudioStreamEndCount(), 1u);
}

TEST(CaptionControllerTest, OnAudioStreamEndAfterStop) {
  TestingPrefServiceSimple pref_service;
  RegisterPrefs(&pref_service);
  auto delegate = std::make_unique<FakeCaptionControllerDelegate>();
  auto* delegate_ptr = delegate.get();
  CaptionController caption_controller(
      /*caption_bubble_context=*/nullptr, &pref_service, kApplicationLocale,
      std::move(delegate));

  caption_controller.StartLiveCaption();
  caption_controller.StopLiveCaption();
  caption_controller.OnAudioStreamEnd();

  EXPECT_EQ(delegate_ptr->GetOnAudioStreamEndCount(), 0u);
}

TEST(CaptionControllerTest, OnCaptionStyleUpdated) {
  TestingPrefServiceSimple pref_service;
  RegisterPrefs(&pref_service);
  auto delegate = std::make_unique<FakeCaptionControllerDelegate>();
  auto* delegate_ptr = delegate.get();
  CaptionController caption_controller(
      /*caption_bubble_context=*/nullptr, &pref_service, kApplicationLocale,
      std::move(delegate));

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
  RegisterPrefs(&pref_service);
  auto delegate = std::make_unique<FakeCaptionControllerDelegate>();
  auto* delegate_ptr = delegate.get();
  CaptionController caption_controller(
      /*caption_bubble_context=*/nullptr, &pref_service, kApplicationLocale,
      std::move(delegate));

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
  RegisterPrefs(&pref_service);
  auto delegate = std::make_unique<FakeCaptionControllerDelegate>();
  auto* delegate_ptr = delegate.get();
  CaptionController caption_controller(
      /*caption_bubble_context=*/nullptr, &pref_service, kApplicationLocale,
      std::move(delegate));

  caption_controller.StartLiveCaption();
  caption_controller.StopLiveCaption();
  pref_service.SetUserPref(prefs::kAccessibilityCaptionsTextSize,
                           base::Value("10%"));

  EXPECT_THAT(delegate_ptr->GetCaptionStyleObserver(), testing::IsNull());
}

TEST(CaptionControllerTest, DispatchTranscriptionFailed) {
  TestingPrefServiceSimple pref_service;
  RegisterPrefs(&pref_service);
  auto delegate = std::make_unique<FakeCaptionControllerDelegate>();
  auto* delegate_ptr = delegate.get();
  CaptionController caption_controller(
      /*caption_bubble_context=*/nullptr, &pref_service, kApplicationLocale,
      std::move(delegate));

  caption_controller.StartLiveCaption();
  delegate_ptr->SetOnTranscriptionSuccess(/*success=*/false);
  media::SpeechRecognitionResult transcript("transcript", /*is_final=*/true);
  bool success = caption_controller.DispatchTranscription(transcript);

  EXPECT_FALSE(success);
  EXPECT_EQ(delegate_ptr->GetCreateBubbleControllerCount(), 2u);
  ASSERT_EQ(delegate_ptr->GetTranscriptions().size(), 2u);
  EXPECT_EQ(delegate_ptr->GetTranscriptions().at(0), transcript);
  EXPECT_EQ(delegate_ptr->GetTranscriptions().at(1), transcript);
}

}  // namespace
}  // namespace ash::babelorca
