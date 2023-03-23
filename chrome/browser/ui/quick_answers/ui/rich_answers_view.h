// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RICH_ANSWERS_VIEW_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RICH_ANSWERS_VIEW_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_focus_search.h"
#include "chrome/browser/ui/quick_answers/ui/rich_answers_pre_target_handler.h"
#include "ui/events/event_handler.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
}  // namespace views

class QuickAnswersUiController;

namespace quick_answers {

struct QuickAnswer;
class RichAnswersPreTargetHandler;

// A bubble style view to show RichAnswer.
class RichAnswersView : public views::View {
 public:
  static constexpr char kWidgetName[] = "RichAnswersViewWidget";

  RichAnswersView(const gfx::Rect& anchor_view_bounds,
                  base::WeakPtr<QuickAnswersUiController> controller,
                  const quick_answers::QuickAnswer& result);

  RichAnswersView(const RichAnswersView&) = delete;
  RichAnswersView& operator=(const RichAnswersView&) = delete;

  ~RichAnswersView() override;

  // views::View:
  const char* GetClassName() const override;
  void OnFocus() override;
  void OnThemeChanged() override;
  views::FocusTraversable* GetPaneFocusTraversable() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  void InitLayout(const quick_answers::QuickAnswer& result);
  void InitWidget();
  void AddFrameButtons();
  void UpdateBounds();

  // QuickAnswersFocusSearch::GetFocusableViewsCallback to poll currently
  // focusable views.
  std::vector<views::View*> GetFocusableViews();

  gfx::Rect anchor_view_bounds_;

  base::WeakPtr<QuickAnswersUiController> controller_;

  raw_ptr<views::View> base_view_ = nullptr;
  raw_ptr<views::View> content_view_ = nullptr;
  raw_ptr<views::ImageButton> settings_button_ = nullptr;

  std::unique_ptr<quick_answers::RichAnswersPreTargetHandler>
      rich_answers_view_handler_;
  std::unique_ptr<QuickAnswersFocusSearch> focus_search_;
  base::WeakPtrFactory<RichAnswersView> weak_factory_{this};
};

}  // namespace quick_answers

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RICH_ANSWERS_VIEW_H_
