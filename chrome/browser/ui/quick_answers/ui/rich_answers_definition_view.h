// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RICH_ANSWERS_DEFINITION_VIEW_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RICH_ANSWERS_DEFINITION_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/quick_answers/ui/rich_answers_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"

namespace quick_answers {

// A bubble style view to show QuickAnswer.
class RichAnswersDefinitionView : public RichAnswersView {
  METADATA_HEADER(RichAnswersDefinitionView, RichAnswersView)

 public:
  RichAnswersDefinitionView(const gfx::Rect& anchor_view_bounds,
                            base::WeakPtr<QuickAnswersUiController> controller,
                            const DefinitionResult& definition_result);

  RichAnswersDefinitionView(const RichAnswersDefinitionView&) = delete;
  RichAnswersDefinitionView& operator=(const RichAnswersDefinitionView&) =
      delete;

  ~RichAnswersDefinitionView() override;

 private:
  void InitLayout();
  void AddHeaderViews();
  void AddWordClass();
  void SetUpSubContentView();
  void AddDefinition(views::View* container_view,
                     const Sense& sense,
                     int label_width);
  void MaybeAddSampleSentence(views::View* container_view,
                              const Sense& sense,
                              int label_width);
  void MaybeAddSynonyms(views::View* container_view,
                        const Sense& sense,
                        int label_width);
  void MaybeAddAdditionalDefinitions();
  void AddSubsense(const Sense& subsense);
  void AddPhoneticsAudioButtonTo(views::View* container_view);
  void OnPhoneticsAudioButtonPressed();

  raw_ptr<views::View> content_view_ = nullptr;
  raw_ptr<views::BoxLayoutView> subcontent_view_ = nullptr;
  raw_ptr<views::WebView> tts_audio_web_view_ = nullptr;

  DefinitionResult definition_result_;

  base::WeakPtrFactory<RichAnswersDefinitionView> weak_factory_{this};
};

}  // namespace quick_answers

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RICH_ANSWERS_DEFINITION_VIEW_H_
