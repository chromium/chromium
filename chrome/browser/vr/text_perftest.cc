// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "cc/base/lap_timer.h"
#include "chrome/browser/vr/elements/text.h"
#include "chrome/browser/vr/skia_surface_provider_factory.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/test/gl_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace vr {

namespace {

constexpr size_t kNumberOfRuns = 35;
constexpr float kFontHeightMeters = 0.05f;
constexpr float kTextWidthMeters = 1.0f;

}  // namespace

class TextPerfTest : public testing::Test {
 public:
  void SetUp() override {
    gl_test_environment_ =
        std::make_unique<GlTestEnvironment>(kPixelHalfScreen);
    provider_ = SkiaSurfaceProviderFactory::Create();

    text_element_ = std::make_unique<Text>(kFontHeightMeters);
    text_element_->SetFieldWidth(kTextWidthMeters);
    text_element_->Initialize(provider_.get());
  }

  void TearDown() override {
    text_element_.reset();
    provider_.reset();
    gl_test_environment_.reset();
  }

 protected:
  void PrintResults(const std::string& name) {
    perf_test::PrintResult("TextPerfTest", ".render_time_avg", name,
                           timer_.MsPerLap(), "ms", true);
    perf_test::PrintResult("TextPerfTest", ".number_of_runs", name,
                           static_cast<size_t>(timer_.NumLaps()), "runs", true);
  }

  void RenderAndLapTimer() {
    text_element_->PrepareToDrawForTest();
    text_element_->UpdateTexture();
    // Make sure all GL commands are applied before we measure the time.
    glFinish();
    timer_.NextLap();
  }

  std::unique_ptr<Text> text_element_;
  cc::LapTimer timer_;

 private:
  std::unique_ptr<SkiaSurfaceProvider> provider_;
  std::unique_ptr<GlTestEnvironment> gl_test_environment_;
};

TEST_F(TextPerfTest, RenderLoremIpsum100Chars) {
  base::string16 text = base::UTF8ToUTF16(kLoremIpsum100Chars);
  timer_.Reset();
  for (size_t i = 0; i < kNumberOfRuns; i++) {
    text[0] = 'a' + (i % 26);
    text_element_->SetText(text);
    RenderAndLapTimer();
  }
  PrintResults("render_lorem_ipsum_100_chars");
}

TEST_F(TextPerfTest, RenderLoremIpsum700Chars) {
  base::string16 text = base::UTF8ToUTF16(kLoremIpsum700Chars);
  timer_.Reset();
  for (size_t i = 0; i < kNumberOfRuns; i++) {
    text[0] = 'a' + (i % 26);
    text_element_->SetText(text);
    RenderAndLapTimer();
  }
  PrintResults("render_lorem_ipsum_700_chars");
}

}  // namespace vr
