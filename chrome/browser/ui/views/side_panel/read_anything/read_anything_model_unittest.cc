// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_model.h"

#include <vector>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/common/accessibility/read_anything_constants.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "testing/gmock/include/gmock/gmock.h"

#include "ui/accessibility/accessibility_features.h"

using testing::_;
using testing::FloatNear;

class MockReadAnythingModelObserver : public ReadAnythingModel::Observer {
 public:
  MOCK_METHOD(void,
              OnReadAnythingThemeChanged,
              (const std::string& font_name,
               double font_scale,
               ui::ColorId foreground_color_id,
               ui::ColorId background_color_id,
               ui::ColorId separator_color_id,
               ui::ColorId dropdown_color_id,
               ui::ColorId selected_color_id,
               read_anything::mojom::LineSpacing line_spacing,
               read_anything::mojom::LetterSpacing letter_spacing),
              (override));
};

class ReadAnythingModelTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures({features::kReadAnything}, {});
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

  EXPECT_CALL(model_observer_1_,
              OnReadAnythingThemeChanged(_, _, _, _, _, _, _, _, _))
      .Times(1);

  EXPECT_CALL(model_observer_2_,
              OnReadAnythingThemeChanged(_, _, _, _, _, _, _, _, _))
      .Times(1);

  model_->AddObserver(&model_observer_2_);
}

TEST_F(ReadAnythingModelTest, RemovedModelObserversDoNotReceiveNotifications) {
  model_->AddObserver(&model_observer_1_);
  model_->AddObserver(&model_observer_2_);

  EXPECT_CALL(model_observer_1_,
              OnReadAnythingThemeChanged(_, _, _, _, _, _, _, _, _))
      .Times(1);

  EXPECT_CALL(model_observer_2_,
              OnReadAnythingThemeChanged(_, _, _, _, _, _, _, _, _))
      .Times(0);

  EXPECT_CALL(model_observer_3_,
              OnReadAnythingThemeChanged(_, _, _, _, _, _, _, _, _))
      .Times(1);

  model_->RemoveObserver(&model_observer_2_);
  model_->AddObserver(&model_observer_3_);
}

TEST_F(ReadAnythingModelTest, NotificationsOnSetSelectedFontIndex) {
  model_->AddObserver(&model_observer_1_);

  EXPECT_CALL(model_observer_1_,
              OnReadAnythingThemeChanged(_, _, _, _, _, _, _, _, _))
      .Times(1);

  model_->SetSelectedFontByIndex(2);
}

TEST_F(ReadAnythingModelTest, NotificationsOnDecreasedFontSize) {
  model_->AddObserver(&model_observer_1_);

  EXPECT_CALL(model_observer_1_,
              OnReadAnythingThemeChanged(_, _, _, _, _, _, _, _, _))
      .Times(1);

  model_->DecreaseTextSize();

  EXPECT_NEAR(model_->GetFontScale(), 0.75, 0.01);
}

TEST_F(ReadAnythingModelTest, NotificationsOnIncreasedFontSize) {
  model_->AddObserver(&model_observer_1_);

  EXPECT_CALL(model_observer_1_,
              OnReadAnythingThemeChanged(_, _, _, _, _, _, _, _, _))
      .Times(1);

  model_->IncreaseTextSize();

  EXPECT_NEAR(model_->GetFontScale(), 1.25, 0.01);
}

TEST_F(ReadAnythingModelTest, NotificationsOnSetSelectedColorsIndex) {
  model_->AddObserver(&model_observer_1_);

  EXPECT_CALL(model_observer_1_,
              OnReadAnythingThemeChanged(_, _, _, _, _, _, _, _, _))
      .Times(1);

  model_->SetSelectedColorsByIndex(2);
}

TEST_F(ReadAnythingModelTest, NotificationsOnSetSelectedLineSpacingIndex) {
  model_->AddObserver(&model_observer_1_);

  EXPECT_CALL(model_observer_1_,
              OnReadAnythingThemeChanged(_, _, _, _, _, _, _, _, _))
      .Times(1);

  model_->SetSelectedLineSpacingByIndex(2);
}

TEST_F(ReadAnythingModelTest, NotificationsOnSetSelectedLetterSpacingIndex) {
  model_->AddObserver(&model_observer_1_);

  EXPECT_CALL(model_observer_1_,
              OnReadAnythingThemeChanged(_, _, _, _, _, _, _, _, _))
      .Times(1);

  model_->SetSelectedLetterSpacingByIndex(2);
}

TEST_F(ReadAnythingModelTest, NotificationsOnSystemThemeChanged) {
  model_->AddObserver(&model_observer_1_);

  EXPECT_CALL(model_observer_1_,
              OnReadAnythingThemeChanged(_, _, _, _, _, _, _, _, _))
      .Times(1);

  model_->OnSystemThemeChanged();
}

TEST_F(ReadAnythingModelTest, MinimumFontScaleIsEnforced) {
  std::string font_name;
  model_->Init(font_name, 0.5, read_anything::mojom::Colors::kDefaultValue,
               read_anything::mojom::LineSpacing::kLoose,
               read_anything::mojom::LetterSpacing::kStandard);
  model_->DecreaseTextSize();
  EXPECT_NEAR(model_->GetFontScale(), 0.5, 0.01);
}

TEST_F(ReadAnythingModelTest, MaximumFontScaleIsEnforced) {
  std::string font_name;
  model_->Init(font_name, 4.5, read_anything::mojom::Colors::kDefaultValue,
               read_anything::mojom::LineSpacing::kLoose,
               read_anything::mojom::LetterSpacing::kStandard);
  model_->IncreaseTextSize();
  EXPECT_NEAR(model_->GetFontScale(), 4.5, 0.01);
}

TEST_F(ReadAnythingModelTest, FontModelIsValidFontName) {
  EXPECT_TRUE(GetFontModel()->IsValidFontName("Standard font"));
  EXPECT_TRUE(GetFontModel()->IsValidFontName("Sans-serif"));
  EXPECT_TRUE(GetFontModel()->IsValidFontName("Serif"));
  EXPECT_TRUE(GetFontModel()->IsValidFontName("Arial"));
  EXPECT_TRUE(GetFontModel()->IsValidFontName("Comic Sans MS"));
  EXPECT_TRUE(GetFontModel()->IsValidFontName("Times New Roman"));
  EXPECT_FALSE(GetFontModel()->IsValidFontName("xxyyzz"));
}

TEST_F(ReadAnythingModelTest, FontModelGetCurrentFontName) {
  EXPECT_EQ("Standard font", GetFontModel()->GetFontNameAt(0));
  EXPECT_EQ("Sans-serif", GetFontModel()->GetFontNameAt(1));
  EXPECT_EQ("Serif", GetFontModel()->GetFontNameAt(2));
  EXPECT_EQ("Arial", GetFontModel()->GetFontNameAt(3));
  EXPECT_EQ("Comic Sans MS", GetFontModel()->GetFontNameAt(4));
  EXPECT_EQ("Times New Roman", GetFontModel()->GetFontNameAt(5));
}

TEST_F(ReadAnythingModelTest, LabelFontListModelGetsCurrentFontList) {
  std::string default_font = string_constants::kReadAnythingDefaultFontName;

  const std::vector<std::string> expected_fonts = {
      "Standard font", "Sans-serif",    "Serif",
      "Arial",         "Comic Sans MS", "Times New Roman"};

  for (size_t i = 0; i < expected_fonts.size(); i++) {
    auto retrieved_fonts = GetFontModel()->GetLabelFontNameAt(i);
    EXPECT_EQ(expected_fonts[i], retrieved_fonts[0]);
    EXPECT_EQ(default_font, retrieved_fonts[1]);
  }
}

TEST_F(ReadAnythingModelTest, DefaultIndexSetOnSetSelectedFontByIndex) {
  size_t testIndex = 2;
  model_->SetSelectedFontByIndex(testIndex);
  EXPECT_EQ(testIndex, GetFontModel()->GetDefaultIndexForTesting().value());
}

TEST_F(ReadAnythingModelTest, FontModelHasDefaultNullOptColors) {
  EXPECT_FALSE(GetFontModel()->GetDropdownForegroundColorIdAt(0).has_value());
  EXPECT_FALSE(GetFontModel()->GetDropdownBackgroundColorIdAt(0).has_value());
  EXPECT_FALSE(
      GetFontModel()->GetDropdownSelectedBackgroundColorIdAt(0).has_value());
}

TEST_F(ReadAnythingModelTest, FontModelSetsDropdownAndForegroundColors) {
  ReadAnythingColorsModel* color_model = model_->GetColorsModel();
  ReadAnythingColorsModel::ColorInfo color_info = color_model->GetColorsAt(2);

  GetFontModel()->SetForegroundColorId(color_info.foreground_color_id);
  GetFontModel()->SetBackgroundColorId(color_info.dropdown_color_id);
  GetFontModel()->SetSelectedBackgroundColorId(
      color_info.selected_dropdown_color_id);

  EXPECT_EQ(color_info.foreground_color_id,
            GetFontModel()->GetDropdownForegroundColorIdAt(0).value());
  EXPECT_EQ(color_info.dropdown_color_id,
            GetFontModel()->GetDropdownBackgroundColorIdAt(0).value());
  EXPECT_EQ(color_info.selected_dropdown_color_id,
            GetFontModel()->GetDropdownSelectedBackgroundColorIdAt(0).value());
}

TEST_F(ReadAnythingModelTest, GetLabelFontList_DoesNotCrashBeforeSet) {
  ReadAnythingColorsModel* color_model = model_->GetColorsModel();
  EXPECT_EQ(nullptr, color_model->GetLabelFontListAt(0));

  ReadAnythingLineSpacingModel* line_spacing_model =
      model_->GetLineSpacingModel();
  EXPECT_EQ(nullptr, line_spacing_model->GetLabelFontListAt(0));

  ReadAnythingLetterSpacingModel* letter_spacing_model =
      model_->GetLetterSpacingModel();
  EXPECT_EQ(nullptr, letter_spacing_model->GetLabelFontListAt(0));
}

TEST_F(ReadAnythingModelTest, GetLabelFontList_GetsCorrectFontList) {
  ReadAnythingColorsModel* color_model = model_->GetColorsModel();
  color_model->SetLabelFontList("Arial");

  const gfx::FontList* font_list = color_model->GetLabelFontListAt(0);
  EXPECT_EQ(2, (int)font_list->GetFonts().size());
  EXPECT_EQ("Arial", font_list->GetPrimaryFont().GetFontName());
}

#endif  // !defined(ADDRESS_SANITIZER)
