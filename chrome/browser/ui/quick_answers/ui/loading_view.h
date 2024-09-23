// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_UI_LOADING_VIEW_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_UI_LOADING_VIEW_H_

#include "chromeos/components/quick_answers/public/cpp/constants.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view_tracker.h"

namespace quick_answers {

class LoadingView : public views::FlexLayoutView {
 public:
  LoadingView();
  ~LoadingView() override = default;

  void SetFirstLineText(const std::u16string& first_line_text);
  std::u16string GetFirstLineText() const;
  void SetDesign(Design design);

 private:
  METADATA_HEADER(LoadingView, views::FlexLayoutView)

  raw_ptr<views::Label> first_line_label_ = nullptr;
  raw_ptr<views::Label> second_line_label_ = nullptr;
};

BEGIN_VIEW_BUILDER(/* no export */, LoadingView, views::FlexLayoutView)
VIEW_BUILDER_PROPERTY(const std::u16string&, FirstLineText)
VIEW_BUILDER_PROPERTY(Design, Design)
END_VIEW_BUILDER

}  // namespace quick_answers

DEFINE_VIEW_BUILDER(/* no export */, quick_answers::LoadingView)

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_UI_LOADING_VIEW_H_
