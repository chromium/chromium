// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RICH_ANSWERS_VIEW_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RICH_ANSWERS_VIEW_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_text_label.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace views {
class ImageButton;
class ImageView;
}  // namespace views

class QuickAnswersUiController;

namespace quick_answers {
struct QuickAnswer;

// A bubble style view to show RichAnswer.
//
// `RichAnswersView` implements the common logic and UI between result-type
// specific cards, e.g. settings button (both UI and on-click handling).
// Subclasses are responsible for populating their UI on `GetContentsView()`.
class RichAnswersView : public views::View, public views::WidgetObserver {
  METADATA_HEADER(RichAnswersView, views::View)

 public:
  static constexpr char kWidgetName[] = "RichAnswersViewWidget";

  RichAnswersView(const RichAnswersView&) = delete;
  RichAnswersView& operator=(const RichAnswersView&) = delete;

  ~RichAnswersView() override;

  static views::UniqueWidgetPtr CreateWidget(
      const gfx::Rect& anchor_view_bounds,
      base::WeakPtr<QuickAnswersUiController> controller,
      const QuickAnswer& quick_answer,
      const StructuredResult& result);

  // views::View:
  void AddedToWidget() override;
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnThemeChanged() override;

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  ui::ImageModel GetIconImageModelForTesting();

 protected:
  RichAnswersView(const gfx::Rect& anchor_view_bounds,
                  base::WeakPtr<QuickAnswersUiController> controller,
                  const ResultType result_type);

  views::View* AddSettingsButtonTo(views::View* container_view);

  void AddHeaderViewsTo(views::View* container_view,
                        const std::string& header_text);

  // Used by subclasses to populate ResultType-specific contents.
  // This will never return nullptr after `RichAnswerView` constructor call.
  views::View* GetContentView();

 private:
  void InitLayout();
  void SetUpBaseView();
  void SetUpMainView();
  void SetUpContentView();
  void AddResultTypeIcon();
  void AddGoogleSearchLink();
  void OnGoogleSearchLinkClicked();
  void UpdateBounds();

  gfx::Rect anchor_view_bounds_;

  base::WeakPtr<QuickAnswersUiController> controller_;

  const ResultType result_type_;

  raw_ptr<views::View> base_view_ = nullptr;
  raw_ptr<views::BoxLayoutView> main_view_ = nullptr;
  raw_ptr<views::BoxLayoutView> content_view_ = nullptr;
  raw_ptr<views::ImageButton> settings_button_ = nullptr;
  raw_ptr<views::ImageView> vector_icon_ = nullptr;
  raw_ptr<views::Link> search_link_label_ = nullptr;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  base::WeakPtrFactory<RichAnswersView> weak_factory_{this};
};

}  // namespace quick_answers

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_UI_RICH_ANSWERS_VIEW_H_
