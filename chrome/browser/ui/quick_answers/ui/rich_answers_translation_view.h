// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RICH_ANSWERS_TRANSLATION_VIEW_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RICH_ANSWERS_TRANSLATION_VIEW_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_focus_search.h"
#include "chrome/browser/ui/quick_answers/ui/rich_answers_view.h"
#include "ui/events/event_handler.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
}  // namespace views

// A bubble style view to show QuickAnswer.
class RichAnswersTranslationView : public views::View {
 public:
  explicit RichAnswersTranslationView(const quick_answers::QuickAnswer& result);

  RichAnswersTranslationView(const RichAnswersTranslationView&) = delete;
  RichAnswersTranslationView& operator=(const RichAnswersTranslationView&) =
      delete;

  ~RichAnswersTranslationView() override;

  // views::View:
  const char* GetClassName() const override;
  void OnFocus() override;
  views::FocusTraversable* GetPaneFocusTraversable() override;

 private:
  void InitLayout();

  // QuickAnswersFocusSearch::GetFocusableViewsCallback to poll currently
  // focusable views.
  std::vector<views::View*> GetFocusableViews();

  raw_ptr<views::View> base_view_ = nullptr;
  raw_ptr<views::ImageButton> settings_button_ = nullptr;

  std::unique_ptr<QuickAnswersFocusSearch> focus_search_;
  base::WeakPtrFactory<RichAnswersTranslationView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_VIEW_H_
