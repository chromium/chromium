// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/battery_saver_bubble_view.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/performance_controls/battery_saver_bubble_delegate.h"
#include "chrome/browser/ui/performance_controls/battery_saver_bubble_observer.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/view_class_properties.h"

// static
const char BatterySaverBubbleView::kViewClassName[] = "BatterySaverBubbleView";

// static
views::BubbleDialogModelHost* BatterySaverBubbleView::CreateBubble(
    views::BubbleAnchor anchor,
    views::BubbleBorder::Arrow anchor_position,
    BatterySaverBubbleObserver* observer,
    std::optional<gfx::Rect> anchor_rect) {
  auto bubble_delegate_unique =
      std::make_unique<BatterySaverBubbleDelegate>(observer);
  auto* bubble_delegate = bubble_delegate_unique.get();

  auto dialog_model =
      ui::DialogModel::Builder(std::move(bubble_delegate_unique))
          .SetInternalName(kViewClassName)
          .SetTitle(l10n_util::GetStringUTF16(IDS_BATTERY_SAVER_BUBBLE_TITLE))
          .SetIsAlertDialog()
          .SetDialogDestroyingCallback(
              base::BindOnce(&BatterySaverBubbleDelegate::OnWindowClosing,
                             base::Unretained(bubble_delegate)))
          .AddParagraph(
              ui::DialogModelLabel(IDS_BATTERY_SAVER_BUBBLE_DESCRIPTION)
                  .set_is_secondary()
                  .set_allow_character_break())
          .AddOkButton(base::DoNothing(),
                       ui::DialogModel::Button::Params().SetLabel(
                           l10n_util::GetStringUTF16(IDS_OK)))
          .AddCancelButton(
              base::BindOnce(&BatterySaverBubbleDelegate::OnSessionOffClicked,
                             base::Unretained(bubble_delegate)),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_BATTERY_SAVER_SESSION_TURN_OFF)))
          .Build();

  auto bubble_unique = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), anchor, anchor_position);
  auto* bubble = bubble_unique.get();

  if (anchor_rect.has_value()) {
    bubble->SetAnchorRect(*anchor_rect);
  }

  views::Widget* const widget =
      views::BubbleDialogDelegate::CreateBubbleDeprecated(
          std::move(bubble_unique),
          views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);

  widget->GetContentsView()->SetProperty(views::kElementIdentifierKey,
                                         kToolbarBatterySaverBubbleElementId);
  widget->Show();

  observer->OnBubbleShown();
  return bubble;
}

// static
void BatterySaverBubbleView::CloseBubble(
    views::BubbleDialogModelHost* bubble_dialog) {
  CHECK(bubble_dialog, base::NotFatalUntil::M162);
  bubble_dialog->Close();
}
