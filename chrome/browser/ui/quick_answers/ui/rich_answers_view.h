// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RICH_ANSWERS_VIEW_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RICH_ANSWERS_VIEW_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/quick_answers/ui/rich_answers_pre_target_handler.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event_handler.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace views {
class ImageButton;
class ImageView;
}  // namespace views

namespace chromeos::editor_menu {
class FocusSearch;
}  // namespace chromeos::editor_menu

class QuickAnswersUiController;

namespace quick_answers {
struct QuickAnswer;

class RichAnswersPreTargetHandler;

// A bubble style view to show RichAnswer.
//
// `RichAnswersView` implements the common logic and UI between result-type
// specific cards, e.g. settings button (both UI and on-click handling).
// Subclasses are responsible for populating their UI on `GetContentsView()`.
class RichAnswersView : public views::View {
 public:
  METADATA_HEADER(RichAnswersView);

  static constexpr char kWidgetName[] = "RichAnswersViewWidget";

  RichAnswersView(const RichAnswersView&) = delete;
  RichAnswersView& operator=(const RichAnswersView&) = delete;

  ~RichAnswersView() override;

  static views::UniqueWidgetPtr CreateWidget(
      const gfx::Rect& anchor_view_bounds,
      base::WeakPtr<QuickAnswersUiController> controller,
      const quick_answers::QuickAnswer& result);

  // views::View:
  void OnFocus() override;
  void OnThemeChanged() override;
  views::FocusTraversable* GetPaneFocusTraversable() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  ui::ImageModel GetIconImageModelForTesting();

 protected:
  RichAnswersView(const gfx::Rect& anchor_view_bounds,
                  base::WeakPtr<QuickAnswersUiController> controller,
                  const ResultType result_type);

  void AddSettingsButtonTo(views::View* container_view);

  // FocusSearch::GetFocusableViewsCallback to poll currently focusable views.
  std::vector<views::View*> GetFocusableViews();

  // Used by subclasses to populate ResultType-specific contents.
  // This will never return nullptr after `RichAnswerView` constructor call.
  views::View* GetContentView();

 private:
  void InitLayout();
  void AddResultTypeIcon();
  void AddGoogleSearchLink();
  void OnGoogleSearchLinkClicked();
  void UpdateBounds();

  gfx::Rect anchor_view_bounds_;

  base::WeakPtr<QuickAnswersUiController> controller_;

  const ResultType result_type_;

  raw_ptr<views::View> base_view_ = nullptr;
  raw_ptr<views::View> main_view_ = nullptr;
  raw_ptr<views::View> content_view_ = nullptr;
  raw_ptr<views::ImageButton> settings_button_ = nullptr;
  raw_ptr<views::ImageView> vector_icon_ = nullptr;
  raw_ptr<views::Link> search_link_label_ = nullptr;

  std::unique_ptr<quick_answers::RichAnswersPreTargetHandler>
      rich_answers_view_handler_;
  std::unique_ptr<chromeos::editor_menu::FocusSearch> focus_search_;
  base::WeakPtrFactory<RichAnswersView> weak_factory_{this};
};

}  // namespace quick_answers

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RICH_ANSWERS_VIEW_H_
