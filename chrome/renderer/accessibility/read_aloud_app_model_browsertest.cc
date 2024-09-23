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

  const base::Value::List& EnabledLanguages() {
    return model_->languages_enabled_in_pref();
  }

  void SetLanguageEnabled(const std::string& lang, bool enabled) {
    model_->SetLanguageEnabled(lang, enabled);
  }

  const base::Value::Dict& Voices() { return model_->voices(); }

  void SetVoice(const std::string& voice, const std::string& lang) {
    model_->SetVoice(voice, lang);
  }

  int HighlightGranularity() { return model_->highlight_granularity(); }

  void SetHighlightGranularity(int granularity) {
    model_->set_highlight_granularity(granularity);
  }

  bool IsHighlightOn() { return model_->IsHighlightOn(); }

  std::string DefaultLanguage() { return model_->default_language_code(); }

  void SetDefaultLanguage(std::string lang) {
    model_->set_default_language_code(lang);
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

TEST_F(ReadAnythingReadAloudAppModelTest, EnabledLanguages) {
  EXPECT_TRUE(EnabledLanguages().empty());

  const std::string enabled_lang = "fr";
  SetLanguageEnabled(enabled_lang, true);
  EXPECT_TRUE(base::Contains(EnabledLanguages(), enabled_lang));

  SetLanguageEnabled(enabled_lang, false);
  EXPECT_FALSE(base::Contains(EnabledLanguages(), enabled_lang));
}

TEST_F(ReadAnythingReadAloudAppModelTest, Voices) {
  EXPECT_TRUE(Voices().empty());

  const char* lang1 = "pt-br";
  const char* voice1 = "Mulan";
  const char* lang2 = "yue";
  const char* voice2 = "Shang";
  SetVoice(voice1, lang1);
  SetVoice(voice2, lang2);
  EXPECT_TRUE(base::Contains(Voices(), lang1));
  EXPECT_TRUE(base::Contains(Voices(), lang2));
  EXPECT_STREQ(Voices().FindString(lang1)->c_str(), voice1);
  EXPECT_STREQ(Voices().FindString(lang2)->c_str(), voice2);

  const char* voice3 = "Mushu";
  SetVoice(voice3, lang2);
  EXPECT_TRUE(base::Contains(Voices(), lang1));
  EXPECT_TRUE(base::Contains(Voices(), lang2));
  EXPECT_STREQ(Voices().FindString(lang2)->c_str(), voice3);
}

TEST_F(ReadAnythingReadAloudAppModelTest, Highlight) {
  EXPECT_EQ(HighlightGranularity(), 0);

  const int off = 1;
  SetHighlightGranularity(off);
  EXPECT_EQ(HighlightGranularity(), off);
  EXPECT_FALSE(IsHighlightOn());

  const int on = 0;
  SetHighlightGranularity(on);
  EXPECT_EQ(HighlightGranularity(), on);
  EXPECT_TRUE(IsHighlightOn());
}

TEST_F(ReadAnythingReadAloudAppModelTest, DefaultLanguageCode) {
  EXPECT_EQ(DefaultLanguage(), "en");

  const char* lang1 = "tr";
  SetDefaultLanguage(lang1);
  EXPECT_EQ(DefaultLanguage(), lang1);

  const char* lang2 = "hi";
  SetDefaultLanguage(lang2);
  EXPECT_EQ(DefaultLanguage(), lang2);
}
