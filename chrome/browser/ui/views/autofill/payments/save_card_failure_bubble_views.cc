// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/save_card_failure_bubble_views.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace autofill {

SaveCardFailureBubbleViews::SaveCardFailureBubbleViews(
    views::View* anchor_view,
    content::WebContents* web_contents,
    SaveCardBubbleController* controller)
    : SaveCardBubbleViews(anchor_view, web_contents, controller) {
  DialogDelegate::set_buttons(ui::DIALOG_BUTTON_NONE);
}

std::unique_ptr<views::View>
SaveCardFailureBubbleViews::CreateMainContentView() {
  std::unique_ptr<views::View> main_view = std::make_unique<views::View>();
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  main_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));

  base::string16 explanation = controller()->GetExplanatoryMessage();
  if (!explanation.empty()) {
    auto* explanation_label = new views::Label(
        explanation, CONTEXT_BODY_TEXT_LARGE, views::style::STYLE_SECONDARY);
    explanation_label->SetMultiLine(true);
    explanation_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    main_view->AddChildView(explanation_label);
  }
  return main_view;
}

}  // namespace autofill
