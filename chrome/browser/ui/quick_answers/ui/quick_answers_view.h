// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_VIEW_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_VIEW_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_view.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace views {
class ImageButton;
class ImageView;
class Label;
class LabelButton;
class WebView;
}  // namespace views

namespace chromeos::editor_menu {
class FocusSearch;
}  // namespace chromeos::editor_menu

class QuickAnswersUiController;

namespace quick_answers {
struct QuickAnswer;
struct PhoneticsInfo;

// A bubble style view to show QuickAnswer.
class QuickAnswersView : public chromeos::ReadWriteCardsView {
  METADATA_HEADER(QuickAnswersView, chromeos::ReadWriteCardsView)

 public:
  QuickAnswersView(const std::string& title,
                   bool is_internal,
                   base::WeakPtr<QuickAnswersUiController> controller);

  QuickAnswersView(const QuickAnswersView&) = delete;
  QuickAnswersView& operator=(const QuickAnswersView&) = delete;

  ~QuickAnswersView() override;

  // chromeos::ReadWriteCardsView:
  void RequestFocus() override;
  bool HasFocus() const override;
  void OnFocus() override;
  void OnThemeChanged() override;
  views::FocusTraversable* GetPaneFocusTraversable() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::Size GetMaximumSize() const override;
  void UpdateBoundsForQuickAnswers() override;

  // Called when a click happens to trigger Assistant Query.
  void SendQuickAnswersQuery();

  // Update the quick answers view with quick answers result.
  void UpdateView(const quick_answers::QuickAnswer& quick_answer);

  void ShowRetryView();

  ui::ImageModel GetIconImageModelForTesting();

 private:
  bool HasFocusInside();
  void AddFrameButtons();
  bool ShouldAddPhoneticsAudioButton(ResultType result_type,
                                     GURL phonetics_audio,
                                     bool tts_audio_enabled);
  void AddPhoneticsAudioButton(
      const quick_answers::PhoneticsInfo& phonetics_info,
      View* container);
  int GetLabelWidth(bool is_title);
  void ResetContentView();
  void UpdateQuickAnswerResult(const quick_answers::QuickAnswer& quick_answer);

  // FocusSearch::GetFocusableViewsCallback to poll currently focusable views.
  std::vector<views::View*> GetFocusableViews();

  // Invoked when user clicks the phonetics audio button.
  void OnPhoneticsAudioButtonPressed(
      const quick_answers::PhoneticsInfo& phonetics_info);

  base::WeakPtr<QuickAnswersUiController> controller_;
  std::string title_;
  bool is_internal_ = false;

  views::ViewTracker base_view_;
  views::ViewTracker main_view_;
  views::ViewTracker content_view_;
  views::ViewTracker report_query_view_;
  raw_ptr<views::Label> first_answer_label_ = nullptr;
  raw_ptr<views::LabelButton> retry_label_ = nullptr;
  raw_ptr<views::ImageButton> dogfood_feedback_button_ = nullptr;
  raw_ptr<views::ImageButton> settings_button_ = nullptr;
  raw_ptr<views::ImageButton> phonetics_audio_button_ = nullptr;
  raw_ptr<views::ImageView> result_type_icon_ = nullptr;

  // Invisible WebView to play phonetics audio for definition results. WebView
  // is lazy created to improve performance.
  views::ViewTracker phonetics_audio_web_view_;

  std::unique_ptr<chromeos::editor_menu::FocusSearch> focus_search_;
  base::WeakPtrFactory<QuickAnswersView> weak_factory_{this};
};

}  // namespace quick_answers

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_VIEW_H_
