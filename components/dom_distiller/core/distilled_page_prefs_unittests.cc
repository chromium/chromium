// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distilled_page_prefs.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dom_distiller {

namespace {

class TestingObserver : public DistilledPagePrefs::Observer {
 public:
  TestingObserver()
      : font_(DistilledPagePrefs::FONT_FAMILY_SANS_SERIF),
        theme_(DistilledPagePrefs::THEME_LIGHT),
        scaling_(1.0f) {}

  void OnChangeFontFamily(DistilledPagePrefs::FontFamily new_font) override {
    font_ = new_font;
  }

  DistilledPagePrefs::FontFamily GetFontFamily() { return font_; }

  void OnChangeTheme(DistilledPagePrefs::Theme new_theme) override {
    theme_ = new_theme;
  }

  DistilledPagePrefs::Theme GetTheme() { return theme_; }

  void OnChangeFontScaling(float new_scaling) override {
    scaling_ = new_scaling;
  }

  float GetFontScaling() { return scaling_; }

 private:
  DistilledPagePrefs::FontFamily font_;
  DistilledPagePrefs::Theme theme_;
  float scaling_;
};

}  // namespace

class DistilledPagePrefsTest : public testing::Test {
 protected:
  void SetUp() override {
    pref_service_.reset(new sync_preferences::TestingPrefServiceSyncable());
    DistilledPagePrefs::RegisterProfilePrefs(pref_service_->registry());
    distilled_page_prefs_.reset(new DistilledPagePrefs(pref_service_.get()));
  }

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  std::unique_ptr<DistilledPagePrefs> distilled_page_prefs_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(DistilledPagePrefsTest, TestingOnChangeFontIsBeingCalled) {
  TestingObserver obs;
  distilled_page_prefs_->AddObserver(&obs);

  distilled_page_prefs_->SetFontFamily(
      DistilledPagePrefs::FONT_FAMILY_MONOSPACE);
  EXPECT_EQ(DistilledPagePrefs::FONT_FAMILY_SANS_SERIF, obs.GetFontFamily());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DistilledPagePrefs::FONT_FAMILY_MONOSPACE, obs.GetFontFamily());

  distilled_page_prefs_->SetFontFamily(DistilledPagePrefs::FONT_FAMILY_SERIF);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DistilledPagePrefs::FONT_FAMILY_SERIF, obs.GetFontFamily());
  distilled_page_prefs_->RemoveObserver(&obs);
}

TEST_F(DistilledPagePrefsTest, TestingMultipleObserversFont) {
  TestingObserver obs;
  distilled_page_prefs_->AddObserver(&obs);
  TestingObserver obs2;
  distilled_page_prefs_->AddObserver(&obs2);

  distilled_page_prefs_->SetFontFamily(DistilledPagePrefs::FONT_FAMILY_SERIF);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DistilledPagePrefs::FONT_FAMILY_SERIF, obs.GetFontFamily());
  EXPECT_EQ(DistilledPagePrefs::FONT_FAMILY_SERIF, obs2.GetFontFamily());

  distilled_page_prefs_->RemoveObserver(&obs);

  distilled_page_prefs_->SetFontFamily(
      DistilledPagePrefs::FONT_FAMILY_MONOSPACE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DistilledPagePrefs::FONT_FAMILY_SERIF, obs.GetFontFamily());
  EXPECT_EQ(DistilledPagePrefs::FONT_FAMILY_MONOSPACE, obs2.GetFontFamily());

  distilled_page_prefs_->RemoveObserver(&obs2);
}

TEST_F(DistilledPagePrefsTest, TestingOnChangeThemeIsBeingCalled) {
  TestingObserver obs;
  distilled_page_prefs_->AddObserver(&obs);

  distilled_page_prefs_->SetTheme(DistilledPagePrefs::THEME_SEPIA);
  EXPECT_EQ(DistilledPagePrefs::THEME_LIGHT, obs.GetTheme());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DistilledPagePrefs::THEME_SEPIA, obs.GetTheme());

  distilled_page_prefs_->SetTheme(DistilledPagePrefs::THEME_DARK);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DistilledPagePrefs::THEME_DARK, obs.GetTheme());

  distilled_page_prefs_->RemoveObserver(&obs);
}

TEST_F(DistilledPagePrefsTest, TestingMultipleObserversTheme) {
  TestingObserver obs;
  distilled_page_prefs_->AddObserver(&obs);
  TestingObserver obs2;
  distilled_page_prefs_->AddObserver(&obs2);

  distilled_page_prefs_->SetTheme(DistilledPagePrefs::THEME_SEPIA);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DistilledPagePrefs::THEME_SEPIA, obs.GetTheme());
  EXPECT_EQ(DistilledPagePrefs::THEME_SEPIA, obs2.GetTheme());

  distilled_page_prefs_->RemoveObserver(&obs);

  distilled_page_prefs_->SetTheme(DistilledPagePrefs::THEME_LIGHT);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DistilledPagePrefs::THEME_SEPIA, obs.GetTheme());
  EXPECT_EQ(DistilledPagePrefs::THEME_LIGHT, obs2.GetTheme());

  distilled_page_prefs_->RemoveObserver(&obs2);
}

TEST_F(DistilledPagePrefsTest, TestingOnChangeFontScalingIsBeingCalled) {
  TestingObserver obs;
  distilled_page_prefs_->AddObserver(&obs);

  distilled_page_prefs_->SetFontScaling(1.5f);
  ASSERT_FLOAT_EQ(1.0f, obs.GetFontScaling());
  base::RunLoop().RunUntilIdle();
  ASSERT_FLOAT_EQ(1.5f, obs.GetFontScaling());

  distilled_page_prefs_->SetFontScaling(0.7f);
  base::RunLoop().RunUntilIdle();
  ASSERT_FLOAT_EQ(0.7f, obs.GetFontScaling());

  distilled_page_prefs_->RemoveObserver(&obs);
}

TEST_F(DistilledPagePrefsTest, TestingMultipleObserversFontScaling) {
  TestingObserver obs;
  distilled_page_prefs_->AddObserver(&obs);
  TestingObserver obs2;
  distilled_page_prefs_->AddObserver(&obs2);

  distilled_page_prefs_->SetFontScaling(1.3f);
  base::RunLoop().RunUntilIdle();
  ASSERT_FLOAT_EQ(1.3f, obs.GetFontScaling());
  ASSERT_FLOAT_EQ(1.3f, obs2.GetFontScaling());

  distilled_page_prefs_->RemoveObserver(&obs);

  distilled_page_prefs_->SetFontScaling(0.9f);
  base::RunLoop().RunUntilIdle();
  ASSERT_FLOAT_EQ(1.3f, obs.GetFontScaling());
  ASSERT_FLOAT_EQ(0.9f, obs2.GetFontScaling());

  distilled_page_prefs_->RemoveObserver(&obs2);
}

}  // namespace dom_distiller
