// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_TEXT_LABEL_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_TEXT_LABEL_H_

#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/label.h"

namespace views {
class Label;
}  // namespace views

namespace quick_answers {

class QuickAnswersTextLabel : public views::Label {
  METADATA_HEADER(QuickAnswersTextLabel, views::Label)

 public:
  explicit QuickAnswersTextLabel(const QuickAnswerText& quick_answers_text);

  QuickAnswersTextLabel(const QuickAnswersTextLabel&) = delete;
  QuickAnswersTextLabel& operator=(const QuickAnswersTextLabel&) = delete;

  ~QuickAnswersTextLabel() override = default;

  // Creates a QuickAnswersTextLabel with the specified style arguments.
  static std::unique_ptr<QuickAnswersTextLabel> CreateLabelWithStyle(
      const std::string& text,
      const gfx::FontList& font_list,
      int width,
      bool is_multi_line,
      ui::ColorId enabled_color_id = ui::kColorLabelForeground);
};

}  // namespace quick_answers

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_TEXT_LABEL_H_
