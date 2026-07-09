// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feature_showcase/google_lens_handler.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/lens/lens_search_feature_flag_utils.h"
#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_metrics.h"
#include "chrome/browser/ui/webui/feature_showcase/google_lens.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/lens/lens_permission_user_action.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

class GoogleLensHandlerTest : public testing::Test {
 public:
  GoogleLensHandlerTest() = default;
  ~GoogleLensHandlerTest() override = default;

 protected:
  TestingProfile* profile() { return &profile_; }

  base::HistogramTester histogram_tester_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(GoogleLensHandlerTest, EnablesGoogleLens) {
  mojo::PendingReceiver<feature_showcase::mojom::GoogleLensPageHandler>
      receiver;
  GoogleLensHandler handler(std::move(receiver), profile());

  // Ensure it starts not granted.
  ASSERT_FALSE(lens::DidUserGrantLensOverlayNeededPermissions(profile()));

  handler.EnableGoogleLens();

  // Ensure it is now granted.
  EXPECT_TRUE(lens::DidUserGrantLensOverlayNeededPermissions(profile()));

  histogram_tester_.ExpectUniqueSample(
      "ProfilePicker.FREFlow.FeatureShowcase.StepUserAction.GoogleLens",
      FeatureShowcaseStepUserAction::kAccepted, 1);
  histogram_tester_.ExpectUniqueSample(
      "Lens.Overlay.FirstRunPermissionNotice.UserAction",
      lens::LensPermissionUserAction::kAcceptButtonPressed, 1);
}

TEST_F(GoogleLensHandlerTest, SkipsGoogleLens) {
  mojo::PendingReceiver<feature_showcase::mojom::GoogleLensPageHandler>
      receiver;
  GoogleLensHandler handler(std::move(receiver), profile());

  handler.SkipGoogleLens();

  histogram_tester_.ExpectUniqueSample(
      "ProfilePicker.FREFlow.FeatureShowcase.StepUserAction.GoogleLens",
      FeatureShowcaseStepUserAction::kDeclined, 1);
  histogram_tester_.ExpectUniqueSample(
      "Lens.Overlay.FirstRunPermissionNotice.UserAction",
      lens::LensPermissionUserAction::kCancelButtonPressed, 1);
}

TEST_F(GoogleLensHandlerTest, LogsEscKeyPressedOnDestruction) {
  mojo::PendingReceiver<feature_showcase::mojom::GoogleLensPageHandler>
      receiver;
  {
    GoogleLensHandler handler(std::move(receiver), profile());
  }

  histogram_tester_.ExpectUniqueSample(
      "Lens.Overlay.FirstRunPermissionNotice.UserAction",
      lens::LensPermissionUserAction::kEscKeyPressed, 1);
}
