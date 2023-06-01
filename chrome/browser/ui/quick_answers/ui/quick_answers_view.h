// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_VIEW_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_VIEW_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_focus_search.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event_handler.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace views {
class ImageButton;
class ImageView;
class Label;
class LabelButton;
class WebView;
}  // namespace views

class QuickAnswersUiController;

namespace quick_answers {
struct QuickAnswer;
struct PhoneticsInfo;

class QuickAnswersPreTargetHandler;

// A bubble style view to show QuickAnswer.
class QuickAnswersView : public views::View {
 public:
  METADATA_HEADER(QuickAnswersView);

  static constexpr char kWidgetName[] = "QuickAnswersViewWidget";

  QuickAnswersView(const gfx::Rect& anchor_view_bounds,
                   const std::string& title,
                   bool is_internal,
                   base::WeakPtr<QuickAnswersUiController> controller);

  QuickAnswersView(const QuickAnswersView&) = delete;
  QuickAnswersView& operator=(const QuickAnswersView&) = delete;

  ~QuickAnswersView() override;

  static views::UniqueWidgetPtr CreateWidget(
      const gfx::Rect& anchor_view_bounds,
      const std::string& title,
      bool is_internal,
      base::WeakPtr<QuickAnswersUiController> controller);

  // views::View:
  void RequestFocus() override;
  bool HasFocus() const override;
  void OnFocus() override;
  void OnThemeChanged() override;
  views::FocusTraversable* GetPaneFocusTraversable() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // Called when a click happens to trigger Assistant Query.
  void SendQuickAnswersQuery();

  void UpdateAnchorViewBounds(const gfx::Rect& anchor_view_bounds);

  // Update the quick answers view with quick answers result.
  void UpdateView(const gfx::Rect& anchor_view_bounds,
                  const quick_answers::QuickAnswer& quick_answer);

  void ShowRetryView();

  ui::ImageModel GetIconImageModelForTesting();

  gfx::Rect GetAnchorViewBounds() { return anchor_view_bounds_; }

 private:
  void InitLayout();
  void AddContentView();
  void AddFrameButtons();
  void AddPhoneticsAudioButton(
      const quick_answers::PhoneticsInfo& phonetics_info,
      View* container);
  void AddAssistantIcon();
  void AddGoogleIcon();
  void AddDefaultResultTypeIcon();
  int GetBoundsWidth();
  int GetLabelWidth();
  void ResetContentView();
  void UpdateBounds();
  void UpdateQuickAnswerResult(const quick_answers::QuickAnswer& quick_answer);

  // QuickAnswersFocusSearch::GetFocusableViewsCallback to poll currently
  // focusable views.
  std::vector<views::View*> GetFocusableViews();

  // Invoked when user clicks the phonetics audio button.
  void OnPhoneticsAudioButtonPressed(
      const quick_answers::PhoneticsInfo& phonetics_info);

  // The relative position to the screen for Ash and to the toplevel window for
  // Lacros.
  gfx::Rect anchor_view_bounds_;

  base::WeakPtr<QuickAnswersUiController> controller_;
  bool has_second_row_answer_ = false;
  std::string title_;
  bool is_internal_ = false;

  raw_ptr<views::View> base_view_ = nullptr;
  raw_ptr<views::View> main_view_ = nullptr;
  raw_ptr<views::View> content_view_ = nullptr;
  raw_ptr<views::View> report_query_view_ = nullptr;
  raw_ptr<views::Label> first_answer_label_ = nullptr;
  raw_ptr<views::LabelButton> retry_label_ = nullptr;
  raw_ptr<views::ImageButton> dogfood_feedback_button_ = nullptr;
  raw_ptr<views::ImageButton> settings_button_ = nullptr;
  raw_ptr<views::ImageButton> phonetics_audio_button_ = nullptr;
  raw_ptr<views::ImageView> result_type_icon_ = nullptr;

  // Invisible web view to play phonetics audio for definition results.
  raw_ptr<views::WebView> phonetics_audio_web_view_ = nullptr;

  std::unique_ptr<QuickAnswersPreTargetHandler> quick_answers_view_handler_;
  std::unique_ptr<QuickAnswersFocusSearch> focus_search_;
  base::WeakPtrFactory<QuickAnswersView> weak_factory_{this};
};

}  // namespace quick_answers

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_VIEW_H_
