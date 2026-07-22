// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feature_showcase/gemini_handler.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_metrics.h"
#include "chrome/browser/ui/webui/feature_showcase/gemini.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

class GeminiHandlerTest : public testing::Test {
 public:
  GeminiHandlerTest() = default;
  ~GeminiHandlerTest() override = default;

 protected:
  base::HistogramTester histogram_tester_;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(GeminiHandlerTest, AcceptConsent) {
  mojo::PendingReceiver<feature_showcase::mojom::GeminiPageHandler> receiver;
  GeminiHandler handler(std::move(receiver));

  handler.AcceptConsent();

  histogram_tester_.ExpectUniqueSample(
      "ProfilePicker.FREFlow.FeatureShowcase.StepUserAction.Gemini",
      FeatureShowcaseStepUserAction::kAccepted, 1);
}

TEST_F(GeminiHandlerTest, SkipConsent) {
  mojo::PendingReceiver<feature_showcase::mojom::GeminiPageHandler> receiver;
  GeminiHandler handler(std::move(receiver));

  handler.SkipConsent();

  histogram_tester_.ExpectUniqueSample(
      "ProfilePicker.FREFlow.FeatureShowcase.StepUserAction.Gemini",
      FeatureShowcaseStepUserAction::kDeclined, 1);
}
