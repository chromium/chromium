// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RICH_ANSWERS_UNIT_CONVERSION_VIEW_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RICH_ANSWERS_UNIT_CONVERSION_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/quick_answers/ui/rich_answers_view.h"
#include "ui/views/view.h"

// A bubble style view to show QuickAnswer.
class RichAnswersUnitConversionView : public views::View {
 public:
  explicit RichAnswersUnitConversionView(
      const quick_answers::QuickAnswer& result);

  RichAnswersUnitConversionView(const RichAnswersUnitConversionView&) = delete;
  RichAnswersUnitConversionView& operator=(
      const RichAnswersUnitConversionView&) = delete;

  ~RichAnswersUnitConversionView() override;

  // views::View:
  const char* GetClassName() const override;

 private:
  void InitLayout();

  base::WeakPtrFactory<RichAnswersUnitConversionView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RICH_ANSWERS_UNIT_CONVERSION_VIEW_H_
