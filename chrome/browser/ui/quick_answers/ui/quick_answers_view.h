// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_VIEW_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_VIEW_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_view.h"
#include "chrome/browser/ui/quick_answers/ui/loading_view.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_stage_button.h"
#include "chrome/browser/ui/quick_answers/ui/result_view.h"
#include "chrome/browser/ui/quick_answers/ui/retry_view.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace views {
class ImageButton;
class ImageView;
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
  enum class Design { kCurrent, kRefresh, kMagicBoost };

  // Unlike `ResultType`, `Intent` won't change depending on a response from the
  // backend. e.g., (Intent::kDefinition, ResultType::kNoResult) can happen if
  // our local code thinks we can find a definition but the backend doesn't.
  enum class Intent { kUndefined, kDefinition, kUnitConversion, kTranslation };

  struct Params {
   public:
    std::string title;
    Design design = Design::kCurrent;
    Intent intent = Intent::kUndefined;
    // Set true to show a Google internal variant of Qucik Answers UI.
    bool is_internal = false;
  };

  using MockGenerateTtsCallback =
      base::RepeatingCallback<void(const PhoneticsInfo&)>;

  QuickAnswersView(const Params& params,
                   base::WeakPtr<QuickAnswersUiController> controller);

  QuickAnswersView(const QuickAnswersView&) = delete;
  QuickAnswersView& operator=(const QuickAnswersView&) = delete;

  ~QuickAnswersView() override;

  // chromeos::ReadWriteCardsView:
  void RequestFocus() override;
  bool HasFocus() const override;
  void OnFocus() override;
  views::FocusTraversable* GetPaneFocusTraversable() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::Size GetMaximumSize() const override;
  void UpdateBoundsForQuickAnswers() override;

  // Called when a click happens to trigger Assistant Query.
  void SendQuickAnswersQuery();

  void SetResult(const StructuredResult& structured_result);

  void ShowRetryView();

  LoadingView* GetLoadingViewForTesting() { return loading_view_; }
  RetryView* GetRetryViewForTesting() { return retry_view_; }
  ResultView* GetResultViewForTesting() { return result_view_; }
  void SetMockGenerateTtsCallbackForTesting(
      MockGenerateTtsCallback mock_generate_tts_callback);
  views::ImageButton* GetSettingsButtonForTesting() { return settings_button_; }
  views::ImageButton* GetDogfoodButtonForTesting() { return dogfood_button_; }

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
  void GenerateTts(const PhoneticsInfo& phonetics_info);
  void SwitchTo(views::View* view);

  // FocusSearch::GetFocusableViewsCallback to poll currently focusable views.
  std::vector<views::View*> GetFocusableViews();

  // Invoked when user clicks the phonetics audio button.
  void OnPhoneticsAudioButtonPressed(
      const quick_answers::PhoneticsInfo& phonetics_info);

  base::WeakPtr<QuickAnswersUiController> controller_;
  std::string title_;
  const Design design_;
  const Intent intent_;
  const bool is_internal_;

  raw_ptr<QuickAnswersStageButton> quick_answers_stage_button_ = nullptr;
  raw_ptr<views::ImageView> icon_view_ = nullptr;
  raw_ptr<LoadingView> loading_view_ = nullptr;
  raw_ptr<RetryView> retry_view_ = nullptr;
  raw_ptr<ResultView> result_view_ = nullptr;
  raw_ptr<views::ImageButton> settings_button_ = nullptr;
  raw_ptr<views::ImageButton> dogfood_button_ = nullptr;

  MockGenerateTtsCallback mock_generate_tts_callback_;

  // Invisible WebView to play phonetics audio for definition results. WebView
  // is lazy created to improve performance.
  views::ViewTracker phonetics_audio_web_view_;

  std::unique_ptr<chromeos::editor_menu::FocusSearch> focus_search_;
  base::WeakPtrFactory<QuickAnswersView> weak_factory_{this};
};

}  // namespace quick_answers

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_VIEW_H_
