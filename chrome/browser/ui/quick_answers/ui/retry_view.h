// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RETRY_VIEW_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RETRY_VIEW_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "chromeos/components/quick_answers/public/cpp/constants.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/metadata/view_factory.h"

namespace quick_answers {

class RetryView : public views::FlexLayoutView {
  METADATA_HEADER(RetryView, views::FlexLayoutView)
 public:
  using RetryButtonCallback = base::RepeatingCallback<void()>;

  RetryView();
  ~RetryView() override;

  void SetFirstLineText(const std::u16string& first_line_text);
  std::u16string GetFirstLineText() const;
  void SetRetryButtonCallback(RetryButtonCallback retry_button_callback);
  void SetDesign(Design design);

  views::LabelButton* retry_label_button() const { return retry_label_button_; }

 private:
  void OnRetryButtonPressed();

  raw_ptr<views::Label> first_line_label_ = nullptr;
  raw_ptr<views::Label> second_line_label_ = nullptr;
  raw_ptr<views::LabelButton> retry_label_button_ = nullptr;
  RetryButtonCallback retry_button_callback_;
};

BEGIN_VIEW_BUILDER(/* no export */, RetryView, views::FlexLayoutView)
VIEW_BUILDER_PROPERTY(const std::u16string&, FirstLineText)
VIEW_BUILDER_PROPERTY(RetryView::RetryButtonCallback, RetryButtonCallback)
VIEW_BUILDER_PROPERTY(Design, Design)
END_VIEW_BUILDER

}  // namespace quick_answers

DEFINE_VIEW_BUILDER(/* no export */, quick_answers::RetryView)

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RETRY_VIEW_H_
