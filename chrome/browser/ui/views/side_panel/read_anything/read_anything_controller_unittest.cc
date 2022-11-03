// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_controller.h"

#include <memory>

#include "base/test/gtest_util.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_constants.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_model.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_prefs.h"
#include "testing/gmock/include/gmock/gmock.h"

class ReadAnythingControllerTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    TestWithBrowserView::SetUp();

    model_ = std::make_unique<ReadAnythingModel>();
    controller_ =
        std::make_unique<ReadAnythingController>(model_.get(), browser());

    // Reset prefs to default values for test.
    browser()->profile()->GetPrefs()->SetString(
        prefs::kAccessibilityReadAnythingFontName,
        kReadAnythingDefaultFontName);
    browser()->profile()->GetPrefs()->SetDouble(
        prefs::kAccessibilityReadAnythingFontScale,
        kReadAnythingDefaultFontScale);
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kAccessibilityReadAnythingColorInfo,
        static_cast<int>(read_anything::mojom::Colors::kDefaultValue));
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kAccessibilityReadAnythingLineSpacing,
        static_cast<int>(read_anything::mojom::Spacing::kDefault));
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kAccessibilityReadAnythingLetterSpacing,
        static_cast<int>(read_anything::mojom::Spacing::kDefault));
  }

  void MockOnFontChoiceChanged(int index) {
    controller_->OnFontChoiceChanged(index);
  }

  void MockOnFontSizeChanged(bool increase) {
    controller_->OnFontSizeChanged(increase);
  }

  void MockOnColorsChanged(int index) { controller_->OnColorsChanged(index); }

  void MockOnLineSpacingChanged(int index) {
    controller_->OnLineSpacingChanged(index);
  }

  void MockOnLetterSpacingChanged(int index) {
    controller_->OnLetterSpacingChanged(index);
  }

  void MockModelInit(std::string font_name,
                     double font_scale,
                     read_anything::mojom::Colors colors,
                     read_anything::mojom::Spacing line_spacing,
                     read_anything::mojom::Spacing letter_spacing) {
    model_->Init(font_name, font_scale, colors, line_spacing, letter_spacing);
  }

  std::string GetPrefFontName() {
    return browser()->profile()->GetPrefs()->GetString(
        prefs::kAccessibilityReadAnythingFontName);
  }

  double GetPrefFontScale() {
    return browser()->profile()->GetPrefs()->GetDouble(
        prefs::kAccessibilityReadAnythingFontScale);
  }

  int GetPrefsColors() {
    return browser()->profile()->GetPrefs()->GetInteger(
        prefs::kAccessibilityReadAnythingColorInfo);
  }

  int GetPrefsLineSpacing() {
    return browser()->profile()->GetPrefs()->GetInteger(
        prefs::kAccessibilityReadAnythingLineSpacing);
  }

  int GetPrefsLetterSpacing() {
    return browser()->profile()->GetPrefs()->GetInteger(
        prefs::kAccessibilityReadAnythingLetterSpacing);
  }

 protected:
  std::unique_ptr<ReadAnythingModel> model_;
  std::unique_ptr<ReadAnythingController> controller_;
};

TEST_F(ReadAnythingControllerTest, ValidIndexUpdatesFontNamePref) {
  std::string expected_font_name = "Avenir";

  MockOnFontChoiceChanged(3);

  EXPECT_EQ(expected_font_name, GetPrefFontName());
}

TEST_F(ReadAnythingControllerTest, OnFontSizeChangedIncreaseUpdatesPref) {
  EXPECT_NEAR(GetPrefFontScale(), 1.0, 0.01);

  MockOnFontSizeChanged(true);

  EXPECT_NEAR(GetPrefFontScale(), 1.25, 0.01);
}

TEST_F(ReadAnythingControllerTest, OnFontSizeChangedDecreasePref) {
  EXPECT_NEAR(GetPrefFontScale(), 1.0, 0.01);

  MockOnFontSizeChanged(false);

  EXPECT_NEAR(GetPrefFontScale(), 0.75, 0.01);
}

TEST_F(ReadAnythingControllerTest, OnFontSizeChangedHonorsMax) {
  EXPECT_NEAR(GetPrefFontScale(), 1.0, 0.01);

  std::string font_name;
  MockModelInit(font_name, 4.5, read_anything::mojom::Colors::kDefaultValue,
                read_anything::mojom::Spacing::kDefault,
                read_anything::mojom::Spacing::kDefault);

  MockOnFontSizeChanged(true);

  EXPECT_NEAR(GetPrefFontScale(), 4.5, 0.01);
}

TEST_F(ReadAnythingControllerTest, OnFontSizeChangedHonorsMin) {
  EXPECT_NEAR(GetPrefFontScale(), 1.0, 0.01);

  std::string font_name;
  MockModelInit(font_name, 0.5, read_anything::mojom::Colors::kDefaultValue,
                read_anything::mojom::Spacing::kDefault,
                read_anything::mojom::Spacing::kDefault);

  MockOnFontSizeChanged(false);

  EXPECT_NEAR(GetPrefFontScale(), 0.5, 0.01);
}

TEST_F(ReadAnythingControllerTest, OnColorsChangedUpdatesPref) {
  EXPECT_EQ(GetPrefsColors(), 0);

  MockOnColorsChanged(static_cast<int>(read_anything::mojom::Colors::kYellow));

  EXPECT_EQ(GetPrefsColors(), 3);
}

TEST_F(ReadAnythingControllerTest, OnLineSpacingChangedUpdatesPref) {
  EXPECT_EQ(GetPrefsLineSpacing(), 1);

  MockOnLineSpacingChanged(
      static_cast<int>(read_anything::mojom::Spacing::kLoose));

  EXPECT_EQ(GetPrefsLineSpacing(), 2);
}

TEST_F(ReadAnythingControllerTest, OnLineSpacingChangedInvalidInput) {
  EXPECT_EQ(GetPrefsLineSpacing(), 1);

  MockOnLineSpacingChanged(10);

  EXPECT_EQ(GetPrefsLineSpacing(), 1);
}

TEST_F(ReadAnythingControllerTest, OnLetterSpacingChangedUpdatesPref) {
  EXPECT_EQ(GetPrefsLetterSpacing(), 1);

  MockOnLetterSpacingChanged(
      static_cast<int>(read_anything::mojom::Spacing::kLoose));

  EXPECT_EQ(GetPrefsLetterSpacing(), 2);
}

TEST_F(ReadAnythingControllerTest, OnLetterSpacingChangedInvalidInput) {
  EXPECT_EQ(GetPrefsLetterSpacing(), 1);

  MockOnLetterSpacingChanged(10);

  EXPECT_EQ(GetPrefsLetterSpacing(), 1);
}
