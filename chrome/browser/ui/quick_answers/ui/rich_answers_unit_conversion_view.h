// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RICH_ANSWERS_UNIT_CONVERSION_VIEW_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RICH_ANSWERS_UNIT_CONVERSION_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/quick_answers/ui/rich_answers_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace quick_answers {

// A bubble style view to show QuickAnswer.
class RichAnswersUnitConversionView : public RichAnswersView {
 public:
  METADATA_HEADER(RichAnswersUnitConversionView);

  RichAnswersUnitConversionView(
      const gfx::Rect& anchor_view_bounds,
      base::WeakPtr<QuickAnswersUiController> controller);

  RichAnswersUnitConversionView(const RichAnswersUnitConversionView&) = delete;
  RichAnswersUnitConversionView& operator=(
      const RichAnswersUnitConversionView&) = delete;

  ~RichAnswersUnitConversionView() override;

 private:
  void InitLayout();

  raw_ptr<views::View> content_view_ = nullptr;
  raw_ptr<views::View> title_view_ = nullptr;

  base::WeakPtrFactory<RichAnswersUnitConversionView> weak_factory_{this};
};

}  // namespace quick_answers

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RICH_ANSWERS_UNIT_CONVERSION_VIEW_H_
