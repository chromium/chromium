// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/font_access/font_access_chooser_controller.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chooser_controller/mock_chooser_controller_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

class FontAccessChooserControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  FontAccessChooserControllerTest() {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kFontAccess);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    mock_font_chooser_view_ = std::make_unique<MockChooserControllerView>();
    content::ResetFontEnumerationCache();
  }

 protected:
  std::unique_ptr<MockChooserControllerView> mock_font_chooser_view_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(FontAccessChooserControllerTest, MultiSelectTest) {
  base::RunLoop run_loop;
  FontAccessChooserController controller(
      main_rfh(), /*selection=*/std::vector<std::string>(),
      base::BindLambdaForTesting(
          [&](blink::mojom::FontEnumerationStatus status,
              std::vector<blink::mojom::FontMetadataPtr> items) {
            EXPECT_EQ(status, blink::mojom::FontEnumerationStatus::kOk);
            EXPECT_EQ(items.size(), 2u);
            run_loop.Quit();
          }));

  base::RunLoop readiness_loop;
  controller.SetReadyCallbackForTesting(readiness_loop.QuitClosure());
  readiness_loop.Run();

  controller.set_view(mock_font_chooser_view_.get());
  EXPECT_GT(controller.NumOptions(), 1u)
      << "FontAccessChooserController has more than 2 options";
  controller.Select(std::vector<size_t>({0, 1}));
  run_loop.Run();
}

TEST_F(FontAccessChooserControllerTest, CancelTest) {
  base::RunLoop run_loop;
  FontAccessChooserController controller(
      main_rfh(),
      /*selection=*/std::vector<std::string>(),
      base::BindLambdaForTesting(
          [&](blink::mojom::FontEnumerationStatus status,
              std::vector<blink::mojom::FontMetadataPtr> items) {
            EXPECT_EQ(status, blink::mojom::FontEnumerationStatus::kCanceled);
            EXPECT_EQ(items.size(), 0u);
            run_loop.Quit();
          }));

  base::RunLoop readiness_loop;
  controller.SetReadyCallbackForTesting(readiness_loop.QuitClosure());
  readiness_loop.Run();

  controller.set_view(mock_font_chooser_view_.get());
  controller.Cancel();
  run_loop.Run();
}

TEST_F(FontAccessChooserControllerTest, CloseTest) {
  base::RunLoop run_loop;
  FontAccessChooserController controller(
      main_rfh(),
      /*selection=*/std::vector<std::string>(),
      base::BindLambdaForTesting(
          [&](blink::mojom::FontEnumerationStatus status,
              std::vector<blink::mojom::FontMetadataPtr> items) {
            EXPECT_EQ(status, blink::mojom::FontEnumerationStatus::kCanceled);
            EXPECT_EQ(items.size(), 0u);
            run_loop.Quit();
          }));

  base::RunLoop readiness_loop;
  controller.SetReadyCallbackForTesting(readiness_loop.QuitClosure());
  readiness_loop.Run();

  controller.set_view(mock_font_chooser_view_.get());
  controller.Close();
  run_loop.Run();
}

TEST_F(FontAccessChooserControllerTest, DestructorTest) {
  base::RunLoop run_loop;
  std::unique_ptr<FontAccessChooserController> controller =
      std::make_unique<FontAccessChooserController>(
          main_rfh(),
          /*selection=*/std::vector<std::string>(),
          base::BindLambdaForTesting(
              [&](blink::mojom::FontEnumerationStatus status,
                  std::vector<blink::mojom::FontMetadataPtr> items) {
                EXPECT_EQ(
                    status,
                    blink::mojom::FontEnumerationStatus::kUnexpectedError);
                EXPECT_EQ(items.size(), 0u);
                run_loop.Quit();
              }));

  base::RunLoop readiness_loop;
  controller->SetReadyCallbackForTesting(readiness_loop.QuitClosure());
  readiness_loop.Run();

  controller.reset();
  run_loop.Run();
}
