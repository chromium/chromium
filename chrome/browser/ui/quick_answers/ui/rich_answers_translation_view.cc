// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/rich_answers_translation_view.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/display/screen.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

// RichAnswersTranslationView
// -----------------------------------------------------------

RichAnswersTranslationView::RichAnswersTranslationView(
    const quick_answers::QuickAnswer& result) {
  InitLayout();

  // Focus.
  // We use custom focus behavior for the quick answers views.
  // TODO (b/274665781): Add unit tests for focus behavior.
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  set_suppress_default_focus_handling();
  focus_search_ = std::make_unique<QuickAnswersFocusSearch>(
      this, base::BindRepeating(&RichAnswersTranslationView::GetFocusableViews,
                                base::Unretained(this)));
}

RichAnswersTranslationView::~RichAnswersTranslationView() = default;

const char* RichAnswersTranslationView::GetClassName() const {
  return "RichAnswersTranslationView";
}

void RichAnswersTranslationView::OnFocus() {
  View* wants_focus = focus_search_->FindNextFocusableView(
      /* starting_view= */ nullptr,
      views::FocusSearch::SearchDirection::kForwards,
      views::FocusSearch::TraversalDirection::kDown,
      views::FocusSearch::StartingViewPolicy::kCheckStartingView,
      views::FocusSearch::AnchoredDialogPolicy::kSkipAnchoredDialog,
      /* focus_traversable= */ nullptr,
      /* focus_traversable_view= */ nullptr);
  if (wants_focus != this) {
    wants_focus->RequestFocus();
  } else {
    NotifyAccessibilityEvent(ax::mojom::Event::kFocus, true);
  }
}

views::FocusTraversable* RichAnswersTranslationView::GetPaneFocusTraversable() {
  return focus_search_.get();
}

void RichAnswersTranslationView::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

std::vector<views::View*> RichAnswersTranslationView::GetFocusableViews() {
  std::vector<views::View*> focusable_views;
  focusable_views.push_back(this);

  return focusable_views;
}
