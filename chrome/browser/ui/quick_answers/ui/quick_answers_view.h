// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_VIEW_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_VIEW_H_

#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_view.h"
#include "chrome/browser/ui/quick_answers/ui/loading_view.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_stage_button.h"
#include "chrome/browser/ui/quick_answers/ui/result_view.h"
#include "chrome/browser/ui/quick_answers/ui/retry_view.h"
#include "chromeos/components/quick_answers/public/cpp/constants.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/metadata/view_factory.h"
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
  struct Params {
   public:
    std::string title;
    Design design = Design::kCurrent;
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
  gfx::Size GetMaximumSize() const override;
  void UpdateBoundsForQuickAnswers() override;
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override;

  // Called when a click happens to trigger Assistant Query.
  void SendQuickAnswersQuery();

  // `std::nullopt` means an unknown intent, which is used only from
  // Linux-ChromeOS. On Linux-ChromeOS, intent generation code is not exercised.
  // Intent is initially set to `kUnknown` on Linux-ChromeOS and set it to an
  // actual intent later by the backend, i.e., intent transition `std::nullopt`
  // -> an intent value. There can be a case where pre-process find an intent
  // but the backend doesn't find the intent. For that case, the spec is that we
  // keep the original intent icon/ui, i.e., no transition of an intent value ->
  // `std::nullopt`.
  void SetIntent(Intent intent);
  std::optional<Intent> GetIntent() const;

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

  void UpdateUiText();
  void UpdateViewAccessibility();
  void UpdateIcon();

  base::WeakPtr<QuickAnswersUiController> controller_;
  std::string title_;
  const Design design_;
  std::optional<Intent> intent_ = std::nullopt;
  const bool is_internal_;

  raw_ptr<QuickAnswersStageButton> quick_answers_stage_button_ = nullptr;
  raw_ptr<views::Label> refreshed_ui_header_ = nullptr;

  raw_ptr<LoadingView> loading_view_ = nullptr;
  raw_ptr<RetryView> retry_view_ = nullptr;
  raw_ptr<ResultView> result_view_ = nullptr;
  raw_ptr<views::ImageButton> settings_button_ = nullptr;
  raw_ptr<views::ImageButton> dogfood_button_ = nullptr;

  raw_ptr<views::ImageView> icon_ = nullptr;

  MockGenerateTtsCallback mock_generate_tts_callback_;

  // Invisible WebView to play phonetics audio for definition results. WebView
  // is lazy created to improve performance.
  views::ViewTracker phonetics_audio_web_view_;

  std::unique_ptr<chromeos::editor_menu::FocusSearch> focus_search_;
  base::WeakPtrFactory<QuickAnswersView> weak_factory_{this};
};

BEGIN_VIEW_BUILDER(/* no export */,
                   QuickAnswersView,
                   chromeos::ReadWriteCardsView)
VIEW_BUILDER_PROPERTY(Intent, Intent)
END_VIEW_BUILDER

}  // namespace quick_answers

DEFINE_VIEW_BUILDER(/* no export */, quick_answers::QuickAnswersView)

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_VIEW_H_
