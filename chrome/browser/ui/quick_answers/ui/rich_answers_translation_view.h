// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RICH_ANSWERS_TRANSLATION_VIEW_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RICH_ANSWERS_TRANSLATION_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/quick_answers/ui/rich_answers_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event_handler.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view.h"

namespace quick_answers {

// A bubble style view to show QuickAnswer.
class RichAnswersTranslationView : public RichAnswersView {
  METADATA_HEADER(RichAnswersTranslationView, RichAnswersView)

 public:
  RichAnswersTranslationView(const gfx::Rect& anchor_view_bounds,
                             base::WeakPtr<QuickAnswersUiController> controller,
                             const TranslationResult& translation_result);

  RichAnswersTranslationView(const RichAnswersTranslationView&) = delete;
  RichAnswersTranslationView& operator=(const RichAnswersTranslationView&) =
      delete;

  ~RichAnswersTranslationView() override;

 private:
  void InitLayout();
  void AddLanguageTitle(const std::string& locale, bool is_header_view);
  views::FlexLayoutView* AddLanguageText(const std::string& language_text,
                                         bool maybe_append_buttons);
  void AddReadAndCopyButtons(views::View* container_view);
  void OnReadButtonPressed(const std::string& read_text,
                           const std::string& locale);
  void OnCopyButtonPressed(const std::string& copy_text);

  raw_ptr<views::View> content_view_ = nullptr;
  raw_ptr<views::WebView> tts_audio_web_view_ = nullptr;

  TranslationResult translation_result_;

  base::WeakPtrFactory<RichAnswersTranslationView> weak_factory_{this};
};

}  // namespace quick_answers

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RICH_ANSWERS_TRANSLATION_VIEW_H_
