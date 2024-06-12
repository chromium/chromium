// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_aloud_app_model.h"

#include "base/memory/raw_ptr.h"
#include "chrome/test/base/chrome_render_view_test.h"

class ReadAnythingReadAloudAppModelTest : public ChromeRenderViewTest {
 public:
  ReadAnythingReadAloudAppModelTest() = default;
  ~ReadAnythingReadAloudAppModelTest() override = default;
  ReadAnythingReadAloudAppModelTest(const ReadAnythingReadAloudAppModelTest&) =
      delete;
  ReadAnythingReadAloudAppModelTest& operator=(
      const ReadAnythingReadAloudAppModelTest&) = delete;

  void SetUp() override {
    ChromeRenderViewTest::SetUp();
    model_ = new ReadAloudAppModel();
  }

  bool SpeechPlaying() { return model_->speech_playing(); }

  void SetSpeechPlaying(bool speech_playing) {
    model_->set_speech_playing(speech_playing);
  }

  double SpeechRate() { return model_->speech_rate(); }

  void SetSpeechRate(double speech_rate) {
    model_->set_speech_rate(speech_rate);
  }

 private:
  // ReadAloudAppModel constructor and destructor are private so it's
  // not accessible by std::make_unique.
  raw_ptr<ReadAloudAppModel> model_ = nullptr;
};

TEST_F(ReadAnythingReadAloudAppModelTest, SpeechPlaying) {
  EXPECT_FALSE(SpeechPlaying());

  SetSpeechPlaying(true);
  EXPECT_TRUE(SpeechPlaying());

  SetSpeechPlaying(false);
  EXPECT_FALSE(SpeechPlaying());
}

TEST_F(ReadAnythingReadAloudAppModelTest, SpeechRate) {
  EXPECT_EQ(SpeechRate(), 1);

  const double speech_rate1 = 0.5;
  SetSpeechRate(speech_rate1);
  EXPECT_EQ(SpeechRate(), speech_rate1);

  const double speech_rate2 = 1.2;
  SetSpeechRate(speech_rate2);
  EXPECT_EQ(SpeechRate(), speech_rate2);
}
