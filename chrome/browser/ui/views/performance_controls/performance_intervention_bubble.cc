// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/performance_intervention_bubble.h"

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/performance_controls/performance_intervention_button.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/view_class_properties.h"

namespace {
const char kViewClassName[] = "PerformanceInterventionBubble";
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PerformanceInterventionBubble,
                                      kPerformanceInterventionDialogBody);

// static
views::BubbleDialogModelHost* PerformanceInterventionBubble::CreateBubble(
    Browser* browser,
    PerformanceInterventionButton* anchor_view) {
  auto dialog_model =
      ui::DialogModel::Builder()
          .SetInternalName(kViewClassName)
          .SetTitle(l10n_util::GetStringUTF16(
              IDS_PERFORMANCE_INTERVENTION_DIALOG_TITLE))
          .SetIsAlertDialog()
          .SetDialogDestroyingCallback(
              base::BindOnce(&PerformanceInterventionButton::OnBubbleDestroyed,
                             base::Unretained(anchor_view)))
          .AddParagraph(
              ui::DialogModelLabel(IDS_PERFORMANCE_INTERVENTION_DIALOG_BODY)
                  .set_is_secondary()
                  .set_allow_character_break(),
              std::u16string(), kPerformanceInterventionDialogBody)
          .AddOkButton(
              base::DoNothing(),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_PERFORMANCE_INTERVENTION_DEACTIVATE_TABS_BUTTON)))
          .AddCancelButton(
              base::DoNothing(),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_PERFORMANCE_INTERVENTION_DISMISS_BUTTON)))
          .Build();

  auto bubble_unique = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), anchor_view, views::BubbleBorder::TOP_RIGHT);
  auto* const bubble = bubble_unique.get();

  views::BubbleDialogDelegate::CreateBubble(std::move(bubble_unique))->Show();

  return bubble;
}

// static
void PerformanceInterventionBubble::CloseBubble(
    views::BubbleDialogModelHost* bubble_dialog) {
  CHECK(bubble_dialog);
  bubble_dialog->Close();
}
