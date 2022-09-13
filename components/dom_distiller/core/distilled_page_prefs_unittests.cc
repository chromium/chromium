// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distilled_page_prefs.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dom_distiller {

namespace {

class TestingObserver : public DistilledPagePrefs::Observer {
 public:
  TestingObserver()
      : font_(mojom::FontFamily::kSansSerif), theme_(mojom::Theme::kLight) {}

  void OnChangeFontFamily(mojom::FontFamily new_font) override {
    font_ = new_font;
  }

  mojom::FontFamily GetFontFamily() { return font_; }

  void OnChangeTheme(mojom::Theme new_theme) override { theme_ = new_theme; }

  mojom::Theme GetTheme() { return theme_; }

  void OnChangeFontScaling(float new_scaling) override {
    scaling_ = new_scaling;
  }

  float GetFontScaling() { return scaling_; }

 private:
  mojom::FontFamily font_;
  mojom::Theme theme_;
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

  distilled_page_prefs_->SetFontFamily(mojom::FontFamily::kMonospace);
  EXPECT_EQ(mojom::FontFamily::kSansSerif, obs.GetFontFamily());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::FontFamily::kMonospace, obs.GetFontFamily());

  distilled_page_prefs_->SetFontFamily(mojom::FontFamily::kSerif);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::FontFamily::kSerif, obs.GetFontFamily());
  distilled_page_prefs_->RemoveObserver(&obs);
}

TEST_F(DistilledPagePrefsTest, TestingMultipleObserversFont) {
  TestingObserver obs;
  distilled_page_prefs_->AddObserver(&obs);
  TestingObserver obs2;
  distilled_page_prefs_->AddObserver(&obs2);

  distilled_page_prefs_->SetFontFamily(mojom::FontFamily::kSerif);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::FontFamily::kSerif, obs.GetFontFamily());
  EXPECT_EQ(mojom::FontFamily::kSerif, obs2.GetFontFamily());

  distilled_page_prefs_->RemoveObserver(&obs);

  distilled_page_prefs_->SetFontFamily(mojom::FontFamily::kMonospace);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::FontFamily::kSerif, obs.GetFontFamily());
  EXPECT_EQ(mojom::FontFamily::kMonospace, obs2.GetFontFamily());

  distilled_page_prefs_->RemoveObserver(&obs2);
}

TEST_F(DistilledPagePrefsTest, TestingOnChangeThemeIsBeingCalled) {
  TestingObserver obs;
  distilled_page_prefs_->AddObserver(&obs);

  distilled_page_prefs_->SetTheme(mojom::Theme::kSepia);
  EXPECT_EQ(mojom::Theme::kLight, obs.GetTheme());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::Theme::kSepia, obs.GetTheme());

  distilled_page_prefs_->SetTheme(mojom::Theme::kDark);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::Theme::kDark, obs.GetTheme());

  distilled_page_prefs_->RemoveObserver(&obs);
}

TEST_F(DistilledPagePrefsTest, TestingMultipleObserversTheme) {
  TestingObserver obs;
  distilled_page_prefs_->AddObserver(&obs);
  TestingObserver obs2;
  distilled_page_prefs_->AddObserver(&obs2);

  distilled_page_prefs_->SetTheme(mojom::Theme::kSepia);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::Theme::kSepia, obs.GetTheme());
  EXPECT_EQ(mojom::Theme::kSepia, obs2.GetTheme());

  distilled_page_prefs_->RemoveObserver(&obs);

  distilled_page_prefs_->SetTheme(mojom::Theme::kLight);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::Theme::kSepia, obs.GetTheme());
  EXPECT_EQ(mojom::Theme::kLight, obs2.GetTheme());

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
