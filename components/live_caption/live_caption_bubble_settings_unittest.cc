// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/live_caption_bubble_settings.h"

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/live_caption/caption_bubble_settings.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace captions {
namespace {

constexpr char kArabicLanguage[] = "ar-EG";
constexpr char kEnglishLanguage[] = "en-US";

class MockObserver : public CaptionBubbleSettings::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() override = default;
  MOCK_METHOD(void, OnLiveTranslateEnabledChanged, (), (override));
  MOCK_METHOD(void, OnLiveCaptionLanguageChanged, (), (override));
  MOCK_METHOD(void, OnLiveTranslateTargetLanguageChanged, (), (override));
};

class LiveCaptionBubbleSettingsTest : public testing::Test {
 protected:
  void SetUp() override {
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kLiveCaptionBubbleExpanded, false);
    pref_service_.registry()->RegisterBooleanPref(prefs::kLiveTranslateEnabled,
                                                  false);
    pref_service_.registry()->RegisterBooleanPref(prefs::kLiveCaptionEnabled,
                                                  false);
    pref_service_.registry()->RegisterStringPref(
        prefs::kLiveCaptionLanguageCode, kEnglishLanguage);
    pref_service_.registry()->RegisterStringPref(
        prefs::kLiveTranslateTargetLanguageCode, kEnglishLanguage);
  }

  TestingPrefServiceSimple pref_service_;
  MockObserver observer_;
  base::WeakPtrFactory<MockObserver> observer_weak_ptr_factory_{&observer_};
};

TEST_F(LiveCaptionBubbleSettingsTest, SetLiveCaptionBubbleExpanded) {
  LiveCaptionBubbleSettings caption_bubble_settings(&pref_service_);
  caption_bubble_settings.SetLiveCaptionBubbleExpanded(true);

  EXPECT_TRUE(pref_service_.GetBoolean(prefs::kLiveCaptionBubbleExpanded));
}

TEST_F(LiveCaptionBubbleSettingsTest, SetLiveCaptionEnabled) {
  LiveCaptionBubbleSettings caption_bubble_settings(&pref_service_);
  caption_bubble_settings.SetLiveCaptionEnabled(true);

  EXPECT_TRUE(pref_service_.GetBoolean(prefs::kLiveCaptionEnabled));
}

TEST_F(LiveCaptionBubbleSettingsTest, SetLiveTranslateTargetLanguageCode) {
  LiveCaptionBubbleSettings caption_bubble_settings(&pref_service_);
  caption_bubble_settings.SetLiveTranslateTargetLanguageCode(kArabicLanguage);

  EXPECT_THAT(pref_service_.GetString(prefs::kLiveTranslateTargetLanguageCode),
              testing::StrEq(kArabicLanguage));
}

TEST_F(LiveCaptionBubbleSettingsTest, GetLiveCaptionBubbleExpanded) {
  LiveCaptionBubbleSettings caption_bubble_settings(&pref_service_);
  pref_service_.SetUserPref(prefs::kLiveCaptionBubbleExpanded,
                            base::Value(true));

  EXPECT_TRUE(caption_bubble_settings.GetLiveCaptionBubbleExpanded());
}

TEST_F(LiveCaptionBubbleSettingsTest, GetLiveTranslateEnabled) {
  LiveCaptionBubbleSettings caption_bubble_settings(&pref_service_);
  caption_bubble_settings.SetObserver(observer_weak_ptr_factory_.GetWeakPtr());

  EXPECT_FALSE(caption_bubble_settings.GetLiveTranslateEnabled());

  EXPECT_CALL(observer_, OnLiveTranslateEnabledChanged).Times(1);
  pref_service_.SetUserPref(prefs::kLiveTranslateEnabled, base::Value(true));

  EXPECT_TRUE(caption_bubble_settings.GetLiveTranslateEnabled());
}

TEST_F(LiveCaptionBubbleSettingsTest, GetLiveCaptionLanguageCode) {
  LiveCaptionBubbleSettings caption_bubble_settings(&pref_service_);
  caption_bubble_settings.SetObserver(observer_weak_ptr_factory_.GetWeakPtr());

  EXPECT_THAT(caption_bubble_settings.GetLiveCaptionLanguageCode(),
              testing::StrEq(kEnglishLanguage));

  EXPECT_CALL(observer_, OnLiveCaptionLanguageChanged).Times(1);
  pref_service_.SetUserPref(prefs::kLiveCaptionLanguageCode,
                            base::Value(kArabicLanguage));

  EXPECT_THAT(caption_bubble_settings.GetLiveCaptionLanguageCode(),
              testing::StrEq(kArabicLanguage));
}

TEST_F(LiveCaptionBubbleSettingsTest, GetLiveTranslateTargetLanguageCode) {
  LiveCaptionBubbleSettings caption_bubble_settings(&pref_service_);
  caption_bubble_settings.SetObserver(observer_weak_ptr_factory_.GetWeakPtr());

  EXPECT_THAT(caption_bubble_settings.GetLiveTranslateTargetLanguageCode(),
              testing::StrEq(kEnglishLanguage));

  EXPECT_CALL(observer_, OnLiveTranslateTargetLanguageChanged).Times(1);
  pref_service_.SetUserPref(prefs::kLiveTranslateTargetLanguageCode,
                            base::Value(kArabicLanguage));

  EXPECT_THAT(caption_bubble_settings.GetLiveTranslateTargetLanguageCode(),
              testing::StrEq(kArabicLanguage));
}

TEST_F(LiveCaptionBubbleSettingsTest, RemoveObservation) {
  LiveCaptionBubbleSettings caption_bubble_settings(&pref_service_);
  caption_bubble_settings.SetObserver(observer_weak_ptr_factory_.GetWeakPtr());
  caption_bubble_settings.RemoveObserver();

  EXPECT_CALL(observer_, OnLiveTranslateEnabledChanged).Times(0);
  pref_service_.SetUserPref(prefs::kLiveTranslateEnabled, base::Value(true));
}

}  // namespace
}  // namespace captions
