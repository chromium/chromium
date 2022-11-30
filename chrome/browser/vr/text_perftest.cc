// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/timer/lap_timer.h"
#include "chrome/browser/vr/elements/text.h"
#include "chrome/browser/vr/skia_surface_provider_factory.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/test/gl_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
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
    PrintResults();
    text_element_.reset();
    provider_.reset();
    gl_test_environment_.reset();
  }

 protected:
  void SetupReporter(const std::string& test_name,
                     const std::string& story_name) {
    reporter_ =
        std::make_unique<perf_test::PerfResultReporter>(test_name, story_name);
    reporter_->RegisterImportantMetric(".render_time_avg", "ms");
    reporter_->RegisterImportantMetric(".number_of_runs", "runs");
  }

  void PrintResults() {
    reporter_->AddResult(".render_time_avg", timer_.TimePerLap());
    reporter_->AddResult(".number_of_runs",
                         static_cast<size_t>(timer_.NumLaps()));
  }

  void RenderAndLapTimer() {
    text_element_->PrepareToDrawForTest();
    text_element_->UpdateTexture();
    // Make sure all GL commands are applied before we measure the time.
    glFinish();
    timer_.NextLap();
  }

  std::unique_ptr<Text> text_element_;
  // It would be better to initialize this during SetUp(), but there doesn't
  // appear to be a good way to get the test name from within testing::Test.
  std::unique_ptr<perf_test::PerfResultReporter> reporter_;
  base::LapTimer timer_;

 private:
  std::unique_ptr<SkiaSurfaceProvider> provider_;
  std::unique_ptr<GlTestEnvironment> gl_test_environment_;
};

TEST_F(TextPerfTest, RenderLoremIpsum100Chars) {
  SetupReporter("TextPerfTest", "render_lorem_ipsum_100_chars");
  std::u16string text = base::UTF8ToUTF16(kLoremIpsum100Chars);
  timer_.Reset();
  for (size_t i = 0; i < kNumberOfRuns; i++) {
    text[0] = 'a' + (i % 26);
    text_element_->SetText(text);
    RenderAndLapTimer();
  }
}

TEST_F(TextPerfTest, RenderLoremIpsum700Chars) {
  SetupReporter("TextPerfTest", "render_lorem_ipsum_700_chars");
  std::u16string text = base::UTF8ToUTF16(kLoremIpsum700Chars);
  timer_.Reset();
  for (size_t i = 0; i < kNumberOfRuns; i++) {
    text[0] = 'a' + (i % 26);
    text_element_->SetText(text);
    RenderAndLapTimer();
  }
}

}  // namespace vr
