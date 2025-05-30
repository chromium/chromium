// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/caption_bubble_settings_impl.h"

#include "ash/constants/ash_features.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chromeos/ash/components/boca/babelorca/pref_names.h"
#include "components/live_caption/caption_bubble_settings.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::babelorca {
namespace {

constexpr char kArabicLanguage[] = "ar-EG";
constexpr char kEnglishLanguage[] = "en-US";

class MockObserver : public ::captions::CaptionBubbleSettings::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() override = default;
  MOCK_METHOD(void, OnLiveTranslateEnabledChanged, (), (override));
  MOCK_METHOD(void, OnLiveCaptionLanguageChanged, (), (override));
  MOCK_METHOD(void, OnLiveTranslateTargetLanguageChanged, (), (override));
};

class CaptionBubbleSettingsImplTest : public testing::Test {
 protected:
  void SetUp() override {
    pref_service_.registry()->RegisterBooleanPref(prefs::kCaptionBubbleExpanded,
                                                  false);
    pref_service_.registry()->RegisterStringPref(
        prefs::kTranslateTargetLanguageCode, kEnglishLanguage);
  }

  TestingPrefServiceSimple pref_service_;
  MockObserver observer_;
  base::WeakPtrFactory<MockObserver> observer_weak_ptr_factory_{&observer_};
};

TEST_F(CaptionBubbleSettingsImplTest, SetLiveCaptionBubbleExpanded) {
  CaptionBubbleSettingsImpl caption_bubble_settings(
      &pref_service_, kEnglishLanguage, base::DoNothing());
  caption_bubble_settings.SetLiveCaptionBubbleExpanded(true);

  EXPECT_TRUE(pref_service_.GetBoolean(prefs::kCaptionBubbleExpanded));
}

TEST_F(CaptionBubbleSettingsImplTest, SetLiveTranslateTargetLanguageCode) {
  CaptionBubbleSettingsImpl caption_bubble_settings(
      &pref_service_, kEnglishLanguage, base::DoNothing());
  caption_bubble_settings.SetLiveTranslateTargetLanguageCode(kArabicLanguage);

  EXPECT_THAT(pref_service_.GetString(prefs::kTranslateTargetLanguageCode),
              testing::StrEq(kArabicLanguage));
}

TEST_F(CaptionBubbleSettingsImplTest, SetLiveTranslateEnabled) {
  CaptionBubbleSettingsImpl caption_bubble_settings(
      &pref_service_, kEnglishLanguage, base::DoNothing());
  caption_bubble_settings.SetObserver(observer_weak_ptr_factory_.GetWeakPtr());

  EXPECT_FALSE(caption_bubble_settings.GetLiveTranslateEnabled());
  EXPECT_TRUE(caption_bubble_settings.IsLiveTranslateFeatureEnabled());

  EXPECT_CALL(observer_, OnLiveTranslateEnabledChanged).Times(1);
  caption_bubble_settings.SetLiveTranslateEnabled(true);

  EXPECT_TRUE(caption_bubble_settings.GetLiveTranslateEnabled());
  EXPECT_TRUE(caption_bubble_settings.IsLiveTranslateFeatureEnabled());

  // Another call with the same value for enabled should not notify the
  // observer.
  caption_bubble_settings.SetLiveTranslateEnabled(true);
}

TEST_F(CaptionBubbleSettingsImplTest, GetLiveCaptionBubbleExpanded) {
  CaptionBubbleSettingsImpl caption_bubble_settings(
      &pref_service_, kEnglishLanguage, base::DoNothing());
  pref_service_.SetUserPref(prefs::kCaptionBubbleExpanded, base::Value(true));

  EXPECT_TRUE(caption_bubble_settings.GetLiveCaptionBubbleExpanded());
}

TEST_F(CaptionBubbleSettingsImplTest, GetLiveCaptionLanguageCode) {
  CaptionBubbleSettingsImpl caption_bubble_settings(
      &pref_service_, kEnglishLanguage, base::DoNothing());

  EXPECT_THAT(caption_bubble_settings.GetLiveCaptionLanguageCode(),
              testing::StrEq(kEnglishLanguage));
}

TEST_F(CaptionBubbleSettingsImplTest, GetLiveTranslateTargetLanguageCode) {
  CaptionBubbleSettingsImpl caption_bubble_settings(
      &pref_service_, kEnglishLanguage, base::DoNothing());
  caption_bubble_settings.SetObserver(observer_weak_ptr_factory_.GetWeakPtr());

  EXPECT_THAT(caption_bubble_settings.GetLiveTranslateTargetLanguageCode(),
              testing::StrEq(kEnglishLanguage));

  EXPECT_CALL(observer_, OnLiveTranslateTargetLanguageChanged).Times(1);
  caption_bubble_settings.SetLiveTranslateTargetLanguageCode(kArabicLanguage);

  EXPECT_THAT(caption_bubble_settings.GetLiveTranslateTargetLanguageCode(),
              testing::StrEq(kArabicLanguage));
}

TEST_F(CaptionBubbleSettingsImplTest, RemoveObservation) {
  CaptionBubbleSettingsImpl caption_bubble_settings(
      &pref_service_, kEnglishLanguage, base::DoNothing());
  caption_bubble_settings.SetObserver(observer_weak_ptr_factory_.GetWeakPtr());
  caption_bubble_settings.RemoveObserver();

  EXPECT_CALL(observer_, OnLiveTranslateEnabledChanged).Times(0);
  EXPECT_CALL(observer_, OnLiveTranslateTargetLanguageChanged).Times(0);
  caption_bubble_settings.SetLiveTranslateEnabled(true);
  pref_service_.SetUserPref(prefs::kTranslateTargetLanguageCode,
                            base::Value(kArabicLanguage));
}

TEST_F(CaptionBubbleSettingsImplTest, NotifyWhenCaptionsDisabled) {
  bool notified = false;
  CaptionBubbleSettingsImpl caption_bubble_settings(
      &pref_service_, kEnglishLanguage,
      base::BindLambdaForTesting([&notified]() { notified = true; }));

  caption_bubble_settings.SetLiveCaptionEnabled(/*enabled=*/false);
  EXPECT_TRUE(notified);
}

TEST_F(CaptionBubbleSettingsImplTest, DoesNotNotifyWhenCaptionsEnabled) {
  bool notified = false;
  CaptionBubbleSettingsImpl caption_bubble_settings(
      &pref_service_, kEnglishLanguage,
      base::BindLambdaForTesting([&notified]() { notified = true; }));

  caption_bubble_settings.SetLiveCaptionEnabled(/*enabled=*/true);
  EXPECT_FALSE(notified);
}

TEST_F(CaptionBubbleSettingsImplTest,
       ShouldAdjustPositionOnExpandIfFeatureEnabled) {
  base::test::ScopedFeatureList feature_list(
      {features::kBocaAdjustCaptionBubbleOnExpand});
  CaptionBubbleSettingsImpl caption_bubble_settings(
      &pref_service_, kEnglishLanguage, base::DoNothing());

  EXPECT_TRUE(caption_bubble_settings.ShouldAdjustPositionOnExpand());
}

TEST_F(CaptionBubbleSettingsImplTest,
       ShouldAdjustPositionOnExpandIfFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      {features::kBocaAdjustCaptionBubbleOnExpand});
  CaptionBubbleSettingsImpl caption_bubble_settings(
      &pref_service_, kEnglishLanguage, base::DoNothing());

  EXPECT_FALSE(caption_bubble_settings.ShouldAdjustPositionOnExpand());
}

TEST_F(CaptionBubbleSettingsImplTest, ToggleTranslateAllowed) {
  CaptionBubbleSettingsImpl caption_bubble_settings(
      &pref_service_, kEnglishLanguage, base::DoNothing());
  caption_bubble_settings.SetObserver(observer_weak_ptr_factory_.GetWeakPtr());

  EXPECT_CALL(observer_, OnLiveTranslateEnabledChanged).Times(1);
  caption_bubble_settings.SetTranslateAllowed(false);
  EXPECT_FALSE(caption_bubble_settings.GetTranslateAllowed());

  EXPECT_CALL(observer_, OnLiveTranslateEnabledChanged).Times(1);
  caption_bubble_settings.SetTranslateAllowed(true);
  EXPECT_TRUE(caption_bubble_settings.GetTranslateAllowed());
}

TEST_F(CaptionBubbleSettingsImplTest, GetLiveTranslateEnabled) {
  CaptionBubbleSettingsImpl caption_bubble_settings(
      &pref_service_, kEnglishLanguage, base::DoNothing());
  EXPECT_FALSE(caption_bubble_settings.GetLiveTranslateEnabled());

  // CanEnableTranslate() is initially true.
  caption_bubble_settings.SetLiveTranslateEnabled(true);
  EXPECT_TRUE(caption_bubble_settings.GetLiveTranslateEnabled());

  caption_bubble_settings.SetTranslateAllowed(false);
  EXPECT_FALSE(caption_bubble_settings.GetLiveTranslateEnabled());

  caption_bubble_settings.SetLiveTranslateEnabled(false);
  EXPECT_FALSE(caption_bubble_settings.GetLiveTranslateEnabled());
}

}  // namespace
}  // namespace ash::babelorca
