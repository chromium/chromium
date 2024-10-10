// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RESULT_VIEW_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RESULT_VIEW_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "chromeos/components/quick_answers/public/cpp/constants.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/metadata/view_factory.h"

namespace quick_answers {

class ResultView : public views::FlexLayoutView {
  METADATA_HEADER(ResultView, views::FlexLayoutView)

 public:
  static constexpr int kMaxLines = 2;

  using GenerateTtsCallback =
      base::RepeatingCallback<void(const PhoneticsInfo&)>;

  ResultView();
  ~ResultView() override;

  void SetFirstLineText(const std::u16string& first_line_text);
  std::u16string GetFirstLineText() const;

  // If FirstLineSubText is set, it will be shown as:
  // [FirstLineText] <a dot> [FirstLineSubText]
  // This is used to render [text to translate] + [source locale] where we want
  // to elide [text to translate]. Note that [a word] + [phonetics] are rendered
  // as a single text while it looks similar for a different elide behavior.
  void SetFirstLineSubText(const std::u16string& first_line_sub_text);
  std::u16string GetFirstLineSubText() const;

  void SetPhoneticsInfo(const PhoneticsInfo& phonetics_info);
  PhoneticsInfo GetPhoneticsInfo() const;
  void SetSecondLineText(const std::u16string& second_line_text);
  std::u16string GetSecondLineText() const;
  void SetGenerateTtsCallback(GenerateTtsCallback generate_tts_callback);

  void SetDesign(Design design);

  views::ImageButton* phonetics_audio_button() const {
    return phonetics_audio_button_;
  }

 private:
  void OnPhoneticsAudioButtonPressed();

  PhoneticsInfo phonetics_info_;

  raw_ptr<views::FlexLayoutView> flex_layout_view_ = nullptr;
  raw_ptr<views::Label> first_line_label_ = nullptr;
  raw_ptr<views::Label> separator_label_ = nullptr;
  raw_ptr<views::Label> first_line_sub_label_ = nullptr;
  raw_ptr<views::ImageButton> phonetics_audio_button_ = nullptr;
  raw_ptr<views::Label> second_line_label_ = nullptr;
  GenerateTtsCallback generate_tts_callback_;
};

BEGIN_VIEW_BUILDER(/* no export */, ResultView, views::FlexLayoutView)
VIEW_BUILDER_PROPERTY(const std::u16string&, FirstLineText)
VIEW_BUILDER_PROPERTY(const std::u16string&, FirstLineSubText)
VIEW_BUILDER_PROPERTY(const PhoneticsInfo&, PhoneticsInfo)
VIEW_BUILDER_PROPERTY(const std::u16string&, SecondLineText)
VIEW_BUILDER_PROPERTY(Design, Design)
VIEW_BUILDER_PROPERTY(ResultView::GenerateTtsCallback, GenerateTtsCallback)
END_VIEW_BUILDER

}  // namespace quick_answers

DEFINE_VIEW_BUILDER(/* no export */, quick_answers::ResultView)

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RESULT_VIEW_H_
