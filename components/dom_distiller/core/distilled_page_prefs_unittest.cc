// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distilled_page_prefs.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace dom_distiller {

namespace {

class MockObserver : public DistilledPagePrefs::Observer {
 public:
  MOCK_METHOD(void,
              OnChangeFontFamily,
              (mojom::FontFamily new_font),
              (override));
  MOCK_METHOD(void,
              OnChangeTheme,
              (mojom::Theme new_theme, ThemeSettingsUpdateSource source),
              (override));
  MOCK_METHOD(void, OnChangeFontScaling, (float new_scaling), (override));
};

class TestingObserver : public DistilledPagePrefs::Observer {
 public:
  TestingObserver() = default;

  void OnChangeFontFamily(mojom::FontFamily new_font) override {
    font_ = new_font;
  }

  mojom::FontFamily GetFontFamily() { return font_; }

  void OnChangeTheme(mojom::Theme new_theme,
                     ThemeSettingsUpdateSource source) override {
    theme_ = new_theme;
  }

  mojom::Theme GetTheme() { return theme_; }

  void OnChangeFontScaling(float new_scaling) override {
    scaling_ = new_scaling;
  }

  float GetFontScaling() { return scaling_; }

 private:
  mojom::FontFamily font_ = mojom::FontFamily::kSansSerif;
  mojom::Theme theme_ = mojom::Theme::kLight;
  float scaling_{1.0f};
};

}  // namespace

class DistilledPagePrefsTest : public testing::Test {
 protected:
  void SetUp() override {
    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    DistilledPagePrefs::RegisterProfilePrefs(pref_service_->registry());
    distilled_page_prefs_ =
        std::make_unique<DistilledPagePrefs>(pref_service_.get());
  }

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  std::unique_ptr<DistilledPagePrefs> distilled_page_prefs_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(DistilledPagePrefsTest, TestingOnChangeFontIsBeingCalled) {
  TestingObserver obs;
  distilled_page_prefs_->AddObserver(&obs);
  EXPECT_EQ(mojom::FontFamily::kSansSerif, obs.GetFontFamily());

  distilled_page_prefs_->SetFontFamily(mojom::FontFamily::kMonospace);
  base::RunLoop run_loop1;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop1.QuitClosure());
  run_loop1.Run();
  EXPECT_EQ(mojom::FontFamily::kMonospace, obs.GetFontFamily());

  distilled_page_prefs_->SetFontFamily(mojom::FontFamily::kSerif);
  base::RunLoop run_loop2;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop2.QuitClosure());
  run_loop2.Run();
  EXPECT_EQ(mojom::FontFamily::kSerif, obs.GetFontFamily());
  distilled_page_prefs_->RemoveObserver(&obs);
}

TEST_F(DistilledPagePrefsTest, TestingMultipleObserversFont) {
  TestingObserver obs;
  distilled_page_prefs_->AddObserver(&obs);
  TestingObserver obs2;
  distilled_page_prefs_->AddObserver(&obs2);

  distilled_page_prefs_->SetFontFamily(mojom::FontFamily::kSerif);
  base::RunLoop run_loop1;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop1.QuitClosure());
  run_loop1.Run();
  EXPECT_EQ(mojom::FontFamily::kSerif, obs.GetFontFamily());
  EXPECT_EQ(mojom::FontFamily::kSerif, obs2.GetFontFamily());

  distilled_page_prefs_->RemoveObserver(&obs);

  distilled_page_prefs_->SetFontFamily(mojom::FontFamily::kMonospace);
  base::RunLoop run_loop2;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop2.QuitClosure());
  run_loop2.Run();
  EXPECT_EQ(mojom::FontFamily::kSerif, obs.GetFontFamily());
  EXPECT_EQ(mojom::FontFamily::kMonospace, obs2.GetFontFamily());

  distilled_page_prefs_->RemoveObserver(&obs2);
}

TEST_F(DistilledPagePrefsTest, TestingOnChangeThemeIsBeingCalled) {
  TestingObserver obs;
  distilled_page_prefs_->AddObserver(&obs);
  EXPECT_EQ(mojom::Theme::kLight, obs.GetTheme());

  distilled_page_prefs_->SetUserPrefTheme(mojom::Theme::kSepia);
  base::RunLoop run_loop1;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop1.QuitClosure());
  run_loop1.Run();
  EXPECT_EQ(mojom::Theme::kSepia, obs.GetTheme());

  distilled_page_prefs_->SetUserPrefTheme(mojom::Theme::kDark);
  base::RunLoop run_loop2;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop2.QuitClosure());
  run_loop2.Run();
  EXPECT_EQ(mojom::Theme::kDark, obs.GetTheme());

  distilled_page_prefs_->RemoveObserver(&obs);
}

TEST_F(DistilledPagePrefsTest, TestingOnChangeThemeCalledMultipleTimes) {
  testing::StrictMock<MockObserver> mock_observer;
  distilled_page_prefs_->AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer,
              OnChangeTheme(mojom::Theme::kSepia,
                            ThemeSettingsUpdateSource::kUserPreference));
  distilled_page_prefs_->SetUserPrefTheme(mojom::Theme::kSepia);
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  EXPECT_CALL(mock_observer, OnChangeTheme(mojom::Theme::kSepia, _)).Times(0);
  distilled_page_prefs_->SetUserPrefTheme(mojom::Theme::kSepia);
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  distilled_page_prefs_->RemoveObserver(&mock_observer);
}

TEST_F(DistilledPagePrefsTest, TestingDefaultThemeSet) {
  testing::StrictMock<MockObserver> mock_observer;
  distilled_page_prefs_->AddObserver(&mock_observer);

  // The default theme is set to light by default, no change expected.
  EXPECT_CALL(mock_observer, OnChangeTheme(mojom::Theme::kLight, _)).Times(0);
  distilled_page_prefs_->SetDefaultTheme(mojom::Theme::kLight);
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  EXPECT_CALL(mock_observer,
              OnChangeTheme(mojom::Theme::kDark, ThemeSettingsUpdateSource::kSystem));
  distilled_page_prefs_->SetDefaultTheme(mojom::Theme::kDark);
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  distilled_page_prefs_->RemoveObserver(&mock_observer);
}

TEST_F(DistilledPagePrefsTest, TestingMultipleObserversTheme) {
  TestingObserver obs;
  distilled_page_prefs_->AddObserver(&obs);
  TestingObserver obs2;
  distilled_page_prefs_->AddObserver(&obs2);

  distilled_page_prefs_->SetUserPrefTheme(mojom::Theme::kSepia);
  base::RunLoop run_loop1;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop1.QuitClosure());
  run_loop1.Run();
  EXPECT_EQ(mojom::Theme::kSepia, obs.GetTheme());
  EXPECT_EQ(mojom::Theme::kSepia, obs2.GetTheme());

  distilled_page_prefs_->RemoveObserver(&obs);

  base::RunLoop run_loop2;
  distilled_page_prefs_->SetUserPrefTheme(mojom::Theme::kLight);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop2.QuitClosure());
  run_loop2.Run();
  EXPECT_EQ(mojom::Theme::kSepia, obs.GetTheme());
  EXPECT_EQ(mojom::Theme::kLight, obs2.GetTheme());

  distilled_page_prefs_->RemoveObserver(&obs2);
}

TEST_F(DistilledPagePrefsTest, SetDefaultThemeNoUserPref) {
  TestingObserver obs;
  distilled_page_prefs_->AddObserver(&obs);

  EXPECT_EQ(mojom::Theme::kLight, obs.GetTheme());
  EXPECT_EQ(mojom::Theme::kLight, distilled_page_prefs_->GetTheme());

  distilled_page_prefs_->SetDefaultTheme(mojom::Theme::kDark);
  base::RunLoop run_loop1;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop1.QuitClosure());
  run_loop1.Run();
  EXPECT_EQ(mojom::Theme::kDark, obs.GetTheme());
  EXPECT_EQ(mojom::Theme::kDark, distilled_page_prefs_->GetTheme());

  distilled_page_prefs_->SetDefaultTheme(mojom::Theme::kSepia);
  base::RunLoop run_loop2;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop2.QuitClosure());
  run_loop2.Run();
  EXPECT_EQ(mojom::Theme::kSepia, obs.GetTheme());
  EXPECT_EQ(mojom::Theme::kSepia, distilled_page_prefs_->GetTheme());

  distilled_page_prefs_->RemoveObserver(&obs);
}

TEST_F(DistilledPagePrefsTest, SetDefaultThemeWithUserPref) {
  TestingObserver obs;
  distilled_page_prefs_->AddObserver(&obs);

  base::RunLoop run_loop1;
  distilled_page_prefs_->SetUserPrefTheme(mojom::Theme::kSepia);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop1.QuitClosure());
  run_loop1.Run();
  EXPECT_EQ(mojom::Theme::kSepia, obs.GetTheme());
  EXPECT_EQ(mojom::Theme::kSepia, distilled_page_prefs_->GetTheme());

  distilled_page_prefs_->SetDefaultTheme(mojom::Theme::kDark);
  base::RunLoop run_loop2;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop2.QuitClosure());
  run_loop2.Run();
  EXPECT_EQ(mojom::Theme::kSepia, obs.GetTheme());
  EXPECT_EQ(mojom::Theme::kSepia, distilled_page_prefs_->GetTheme());

  distilled_page_prefs_->RemoveObserver(&obs);
}

TEST_F(DistilledPagePrefsTest, SetUserPrefThemeOverridesSetDefaultTheme) {
  TestingObserver obs;
  distilled_page_prefs_->AddObserver(&obs);

  distilled_page_prefs_->SetDefaultTheme(mojom::Theme::kDark);
  base::RunLoop run_loop1;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop1.QuitClosure());
  run_loop1.Run();
  EXPECT_EQ(mojom::Theme::kDark, obs.GetTheme());
  EXPECT_EQ(mojom::Theme::kDark, distilled_page_prefs_->GetTheme());

  distilled_page_prefs_->SetDefaultTheme(mojom::Theme::kLight);
  base::RunLoop run_loop2;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop2.QuitClosure());
  run_loop2.Run();
  EXPECT_EQ(mojom::Theme::kLight, obs.GetTheme());
  EXPECT_EQ(mojom::Theme::kLight, distilled_page_prefs_->GetTheme());

  distilled_page_prefs_->SetUserPrefTheme(mojom::Theme::kSepia);
  base::RunLoop run_loop3;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop3.QuitClosure());
  run_loop3.Run();
  EXPECT_EQ(mojom::Theme::kSepia, obs.GetTheme());
  EXPECT_EQ(mojom::Theme::kSepia, distilled_page_prefs_->GetTheme());

  distilled_page_prefs_->SetDefaultTheme(mojom::Theme::kLight);
  base::RunLoop run_loop4;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop4.QuitClosure());
  run_loop4.Run();
  EXPECT_EQ(mojom::Theme::kSepia, obs.GetTheme());
  EXPECT_EQ(mojom::Theme::kSepia, distilled_page_prefs_->GetTheme());

  distilled_page_prefs_->RemoveObserver(&obs);
}

TEST_F(DistilledPagePrefsTest, TestingOnChangeFontScalingIsBeingCalled) {
  TestingObserver obs;
  distilled_page_prefs_->AddObserver(&obs);
  ASSERT_FLOAT_EQ(1.0f, obs.GetFontScaling());

  distilled_page_prefs_->SetUserPrefFontScaling(1.5f);
  base::RunLoop run_loop1;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop1.QuitClosure());
  run_loop1.Run();
  ASSERT_FLOAT_EQ(1.5f, obs.GetFontScaling());

  distilled_page_prefs_->SetUserPrefFontScaling(0.7f);
  base::RunLoop run_loop2;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop2.QuitClosure());
  run_loop2.Run();
  ASSERT_FLOAT_EQ(0.7f, obs.GetFontScaling());

  distilled_page_prefs_->RemoveObserver(&obs);
}

TEST_F(DistilledPagePrefsTest, TestingMultipleObserversFontScaling) {
  TestingObserver obs;
  distilled_page_prefs_->AddObserver(&obs);
  TestingObserver obs2;
  distilled_page_prefs_->AddObserver(&obs2);

  distilled_page_prefs_->SetUserPrefFontScaling(1.3f);
  base::RunLoop run_loop1;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop1.QuitClosure());
  run_loop1.Run();
  ASSERT_FLOAT_EQ(1.3f, obs.GetFontScaling());
  ASSERT_FLOAT_EQ(1.3f, obs2.GetFontScaling());

  distilled_page_prefs_->RemoveObserver(&obs);

  distilled_page_prefs_->SetUserPrefFontScaling(0.9f);
  base::RunLoop run_loop2;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop2.QuitClosure());
  run_loop2.Run();
  ASSERT_FLOAT_EQ(1.3f, obs.GetFontScaling());
  ASSERT_FLOAT_EQ(0.9f, obs2.GetFontScaling());

  distilled_page_prefs_->RemoveObserver(&obs2);
}

TEST_F(DistilledPagePrefsTest, SetDefaultFontScalingWithUserPref) {
  TestingObserver obs;
  distilled_page_prefs_->AddObserver(&obs);

  base::RunLoop run_loop1;
  distilled_page_prefs_->SetUserPrefFontScaling(1.5f);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop1.QuitClosure());
  run_loop1.Run();
  ASSERT_FLOAT_EQ(1.5f, obs.GetFontScaling());
  ASSERT_FLOAT_EQ(1.5f, distilled_page_prefs_->GetFontScaling());

  distilled_page_prefs_->SetDefaultFontScaling(1.0f);
  base::RunLoop run_loop2;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop2.QuitClosure());
  run_loop2.Run();
  ASSERT_FLOAT_EQ(1.5f, obs.GetFontScaling());
  ASSERT_FLOAT_EQ(1.5f, distilled_page_prefs_->GetFontScaling());

  distilled_page_prefs_->RemoveObserver(&obs);
}

#if BUILDFLAG(IS_ANDROID)
class DistilledPagePrefsFeatureTest
    : public DistilledPagePrefsTest,
      public ::testing::WithParamInterface<bool> {
 public:
  DistilledPagePrefsFeatureTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(kReaderModeDistillInApp);
    } else {
      scoped_feature_list_.InitAndDisableFeature(kReaderModeDistillInApp);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DistilledPagePrefsTest, SetDefaultFontScalingNoUserPref) {
  TestingObserver obs;
  distilled_page_prefs_->AddObserver(&obs);

  ASSERT_FLOAT_EQ(1.0f, obs.GetFontScaling());
  ASSERT_FLOAT_EQ(1.0f, distilled_page_prefs_->GetFontScaling());

  distilled_page_prefs_->SetDefaultFontScaling(1.5f);
  base::RunLoop run_loop1;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop1.QuitClosure());
  run_loop1.Run();
  ASSERT_FLOAT_EQ(1.5f, obs.GetFontScaling());
  ASSERT_FLOAT_EQ(1.5f, distilled_page_prefs_->GetFontScaling());

  distilled_page_prefs_->SetDefaultFontScaling(2.0f);
  base::RunLoop run_loop2;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop2.QuitClosure());
  run_loop2.Run();
  ASSERT_FLOAT_EQ(2.0f, obs.GetFontScaling());
  ASSERT_FLOAT_EQ(2.0f, distilled_page_prefs_->GetFontScaling());

  distilled_page_prefs_->RemoveObserver(&obs);
}

TEST_F(DistilledPagePrefsTest,
       SetUserPrefFontScalingOverridesSetDefaultFontScaling) {
  TestingObserver obs;
  distilled_page_prefs_->AddObserver(&obs);

  distilled_page_prefs_->SetDefaultFontScaling(1.5f);
  base::RunLoop run_loop1;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop1.QuitClosure());
  run_loop1.Run();
  ASSERT_FLOAT_EQ(1.5f, obs.GetFontScaling());
  ASSERT_FLOAT_EQ(1.5f, distilled_page_prefs_->GetFontScaling());

  distilled_page_prefs_->SetDefaultFontScaling(1.0f);
  base::RunLoop run_loop2;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop2.QuitClosure());
  run_loop2.Run();
  ASSERT_FLOAT_EQ(1.0f, obs.GetFontScaling());
  ASSERT_FLOAT_EQ(1.0f, distilled_page_prefs_->GetFontScaling());

  distilled_page_prefs_->SetUserPrefFontScaling(2.0f);
  base::RunLoop run_loop3;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop3.QuitClosure());
  run_loop3.Run();
  ASSERT_FLOAT_EQ(2.0f, obs.GetFontScaling());
  ASSERT_FLOAT_EQ(2.0f, distilled_page_prefs_->GetFontScaling());

  distilled_page_prefs_->SetDefaultFontScaling(1.0f);
  base::RunLoop run_loop4;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop4.QuitClosure());
  run_loop4.Run();
  ASSERT_FLOAT_EQ(2.0f, obs.GetFontScaling());
  ASSERT_FLOAT_EQ(2.0f, distilled_page_prefs_->GetFontScaling());

  distilled_page_prefs_->RemoveObserver(&obs);
}

TEST_P(DistilledPagePrefsFeatureTest, TestClampDefaultFontScaling) {
  TestingObserver obs;
  distilled_page_prefs_->AddObserver(&obs);

  float min_font_scale = kMinFontScaleAndroidCCT;
  float max_font_scale = kMaxFontScaleAndroidCCT;
  if (GetParam()) {
    min_font_scale = kMinFontScaleAndroidInApp;
    max_font_scale = kMaxFontScaleAndroidInApp;
  }

  // Test clamping for values smaller than the minimum.
  distilled_page_prefs_->SetDefaultFontScaling(min_font_scale - 0.5f);
  base::RunLoop run_loop1;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop1.QuitClosure());
  run_loop1.Run();
  ASSERT_FLOAT_EQ(min_font_scale, obs.GetFontScaling());
  ASSERT_FLOAT_EQ(min_font_scale, distilled_page_prefs_->GetFontScaling());

  // Test clamping for values larger than the maximum.
  distilled_page_prefs_->SetDefaultFontScaling(max_font_scale + 0.5f);
  base::RunLoop run_loop2;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop2.QuitClosure());
  run_loop2.Run();
  ASSERT_FLOAT_EQ(max_font_scale, obs.GetFontScaling());
  ASSERT_FLOAT_EQ(max_font_scale, distilled_page_prefs_->GetFontScaling());

  distilled_page_prefs_->RemoveObserver(&obs);
}

INSTANTIATE_TEST_SUITE_P(All, DistilledPagePrefsFeatureTest, ::testing::Bool());
#endif

}  // namespace dom_distiller
