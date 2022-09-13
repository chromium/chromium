// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/high_efficiency_bubble_view.h"

#include "base/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/performance_controls/high_efficiency_bubble_delegate.h"
#include "chrome/browser/ui/performance_controls/high_efficiency_bubble_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/google_chrome_strings.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/interaction/element_tracker_views.h"

// static
views::BubbleDialogModelHost* HighEfficiencyBubbleView::ShowBubble(
    Browser* browser,
    views::View* anchor_view,
    HighEfficiencyBubbleObserver* observer) {
  auto bubble_delegate_unique =
      std::make_unique<HighEfficiencyBubbleDelegate>(browser, observer);
  auto* bubble_delegate = bubble_delegate_unique.get();

  auto dialog_model =
      ui::DialogModel::Builder(std::move(bubble_delegate_unique))
          .SetTitle(l10n_util::GetStringUTF16(IDS_HIGH_EFFICIENCY_DIALOG_TITLE))
          .SetDialogDestroyingCallback(
              base::BindOnce(&HighEfficiencyBubbleDelegate::OnDialogDestroy,
                             base::Unretained(bubble_delegate)))
          .AddParagraph(ui::DialogModelLabel(IDS_HIGH_EFFICIENCY_DIALOG_BODY)
                            .set_is_secondary())
          .AddOkButton(base::OnceClosure(), l10n_util::GetStringUTF16(IDS_DONE))
          .AddExtraLink(ui::DialogModelLabel::Link(
              IDS_SETTINGS_TITLE,
              base::BindRepeating(
                  &HighEfficiencyBubbleDelegate::OnSettingsClicked,
                  base::Unretained(bubble_delegate))))
          .Build();

  auto bubble_unique = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), anchor_view, views::BubbleBorder::NONE);
  auto* bubble = bubble_unique.get();

  views::Widget* const widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(bubble_unique));
  widget->Show();
  observer->OnBubbleShown();
  return bubble;
}
