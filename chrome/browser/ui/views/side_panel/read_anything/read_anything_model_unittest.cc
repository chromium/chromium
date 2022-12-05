// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_model.h"

#include <vector>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "testing/gmock/include/gmock/gmock.h"

#include "ui/accessibility/accessibility_features.h"

using testing::_;
using testing::FloatNear;

class MockReadAnythingModelObserver : public ReadAnythingModel::Observer {
 public:
  MOCK_METHOD(void,
              OnAXTreeDistilled,
              (const ui::AXTreeUpdate& snapshot,
               const std::vector<ui::AXNodeID>& content_node_ids),
              (override));
  MOCK_METHOD(void,
              OnReadAnythingThemeChanged,
              (const std::string& font_name,
               double font_scale,
               ui::ColorId foreground_color_id,
               ui::ColorId background_color_id,
               read_anything::mojom::Spacing line_spacing,
               read_anything::mojom::Spacing letter_spacing),
              (override));
};

class ReadAnythingModelTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        {features::kUnifiedSidePanel, features::kReadAnything}, {});
    TestWithBrowserView::SetUp();

    model_ = std::make_unique<ReadAnythingModel>();
  }

  // Wrapper methods around the ReadAnythingModel. These do nothing more
  // than keep the below tests less verbose (simple pass-throughs).

  ReadAnythingFontModel* GetFontModel() { return model_->GetFontModel(); }

 protected:
  std::unique_ptr<ReadAnythingModel> model_;

  MockReadAnythingModelObserver model_observer_1_;
  MockReadAnythingModelObserver model_observer_2_;
  MockReadAnythingModelObserver model_observer_3_;
};

// TODO(crbug.com/1344891): Fix the memory leak on destruction observed on these
// tests on asan mac.
#if !BUILDFLAG(IS_MAC) || !defined(ADDRESS_SANITIZER)

TEST_F(ReadAnythingModelTest, AddingModelObserverNotifiesAllObservers) {
  model_->AddObserver(&model_observer_1_);

  EXPECT_CALL(model_observer_1_, OnAXTreeDistilled(_, _)).Times(0);
  EXPECT_CALL(model_observer_1_, OnReadAnythingThemeChanged(_, _, _, _, _, _))
      .Times(1);

  EXPECT_CALL(model_observer_2_, OnAXTreeDistilled(_, _)).Times(0);
  EXPECT_CALL(model_observer_2_, OnReadAnythingThemeChanged(_, _, _, _, _, _))
      .Times(1);

  model_->AddObserver(&model_observer_2_);
}

TEST_F(ReadAnythingModelTest, RemovedModelObserversDoNotReceiveNotifications) {
  model_->AddObserver(&model_observer_1_);
  model_->AddObserver(&model_observer_2_);

  EXPECT_CALL(model_observer_1_, OnAXTreeDistilled(_, _)).Times(0);
  EXPECT_CALL(model_observer_1_, OnReadAnythingThemeChanged(_, _, _, _, _, _))
      .Times(1);

  EXPECT_CALL(model_observer_2_, OnAXTreeDistilled(_, _)).Times(0);
  EXPECT_CALL(model_observer_2_, OnReadAnythingThemeChanged(_, _, _, _, _, _))
      .Times(0);

  EXPECT_CALL(model_observer_3_, OnAXTreeDistilled(_, _)).Times(0);
  EXPECT_CALL(model_observer_3_, OnReadAnythingThemeChanged(_, _, _, _, _, _))
      .Times(1);

  model_->RemoveObserver(&model_observer_2_);
  model_->AddObserver(&model_observer_3_);
}

TEST_F(ReadAnythingModelTest, NotificationsOnSetSelectedFontIndex) {
  model_->AddObserver(&model_observer_1_);

  EXPECT_CALL(model_observer_1_, OnReadAnythingThemeChanged(_, _, _, _, _, _))
      .Times(1);

  model_->SetSelectedFontByIndex(2);
}

TEST_F(ReadAnythingModelTest, NotificationsOnSetDistilledAXTree) {
  model_->AddObserver(&model_observer_1_);

  EXPECT_CALL(model_observer_1_, OnAXTreeDistilled(_, _)).Times(1);

  ui::AXTreeUpdate snapshot_;
  snapshot_.root_id = 1;
  std::vector<ui::AXNodeID> content_node_ids_;
  model_->SetDistilledAXTree(snapshot_, content_node_ids_);
}

TEST_F(ReadAnythingModelTest, NotificationsOnDecreasedFontSize) {
  model_->AddObserver(&model_observer_1_);

  EXPECT_CALL(model_observer_1_, OnReadAnythingThemeChanged(_, _, _, _, _, _))
      .Times(1);

  model_->DecreaseTextSize();

  EXPECT_NEAR(model_->GetFontScale(), 0.75, 0.01);
}

TEST_F(ReadAnythingModelTest, NotificationsOnIncreasedFontSize) {
  model_->AddObserver(&model_observer_1_);

  EXPECT_CALL(model_observer_1_, OnReadAnythingThemeChanged(_, _, _, _, _, _))
      .Times(1);

  model_->IncreaseTextSize();

  EXPECT_NEAR(model_->GetFontScale(), 1.25, 0.01);
}

TEST_F(ReadAnythingModelTest, NotificationsOnSetSelectedColorsIndex) {
  model_->AddObserver(&model_observer_1_);

  EXPECT_CALL(model_observer_1_, OnReadAnythingThemeChanged(_, _, _, _, _, _))
      .Times(1);

  model_->SetSelectedColorsByIndex(2);
}

TEST_F(ReadAnythingModelTest, NotificationsOnSetSelectedLineSpacingIndex) {
  model_->AddObserver(&model_observer_1_);

  EXPECT_CALL(model_observer_1_, OnReadAnythingThemeChanged(_, _, _, _, _, _))
      .Times(1);

  model_->SetSelectedLineSpacingByIndex(2);
}

TEST_F(ReadAnythingModelTest, NotificationsOnSetSelectedLetterSpacingIndex) {
  model_->AddObserver(&model_observer_1_);

  EXPECT_CALL(model_observer_1_, OnReadAnythingThemeChanged(_, _, _, _, _, _))
      .Times(1);

  model_->SetSelectedLetterSpacingByIndex(2);
}

TEST_F(ReadAnythingModelTest, MinimumFontScaleIsEnforced) {
  std::string font_name;
  model_->Init(font_name, 0.5, read_anything::mojom::Colors::kDefaultValue,
               read_anything::mojom::Spacing::kDefault,
               read_anything::mojom::Spacing::kDefault);
  model_->DecreaseTextSize();
  EXPECT_NEAR(model_->GetFontScale(), 0.5, 0.01);
}

TEST_F(ReadAnythingModelTest, MaximumFontScaleIsEnforced) {
  std::string font_name;
  model_->Init(font_name, 4.5, read_anything::mojom::Colors::kDefaultValue,
               read_anything::mojom::Spacing::kDefault,
               read_anything::mojom::Spacing::kDefault);
  model_->IncreaseTextSize();
  EXPECT_NEAR(model_->GetFontScale(), 4.5, 0.01);
}

TEST_F(ReadAnythingModelTest, FontModelIsValidFontName) {
  EXPECT_TRUE(GetFontModel()->IsValidFontName("Standard font"));
  EXPECT_TRUE(GetFontModel()->IsValidFontName("Sans-serif"));
  EXPECT_TRUE(GetFontModel()->IsValidFontName("Serif"));
  EXPECT_TRUE(GetFontModel()->IsValidFontName("Avenir"));
  EXPECT_TRUE(GetFontModel()->IsValidFontName("Comic Neue"));
  EXPECT_TRUE(GetFontModel()->IsValidFontName("Comic Sans MS"));
  EXPECT_TRUE(GetFontModel()->IsValidFontName("Poppins"));
  EXPECT_FALSE(GetFontModel()->IsValidFontName("xxyyzz"));
}

TEST_F(ReadAnythingModelTest, FontModelGetCurrentFontName) {
  EXPECT_EQ("Standard font", GetFontModel()->GetFontNameAt(0));
  EXPECT_EQ("Sans-serif", GetFontModel()->GetFontNameAt(1));
  EXPECT_EQ("Serif", GetFontModel()->GetFontNameAt(2));
  EXPECT_EQ("Avenir", GetFontModel()->GetFontNameAt(3));
  EXPECT_EQ("Comic Neue", GetFontModel()->GetFontNameAt(4));
  EXPECT_EQ("Comic Sans MS", GetFontModel()->GetFontNameAt(5));
  EXPECT_EQ("Poppins", GetFontModel()->GetFontNameAt(6));
}

#endif  // !defined(ADDRESS_SANITIZER)
