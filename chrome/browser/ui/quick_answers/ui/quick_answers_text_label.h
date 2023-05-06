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
 public:
  METADATA_HEADER(QuickAnswersTextLabel);

  explicit QuickAnswersTextLabel(const QuickAnswerText& quick_answers_text);

  QuickAnswersTextLabel(const QuickAnswersTextLabel&) = delete;
  QuickAnswersTextLabel& operator=(const QuickAnswersTextLabel&) = delete;

  ~QuickAnswersTextLabel() override = default;
};

}  // namespace quick_answers

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_TEXT_LABEL_H_
