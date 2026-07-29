// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/performance_intervention_bubble.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/browser/ui/performance_controls/performance_intervention_bubble_delegate.h"
#include "chrome/browser/ui/performance_controls/performance_intervention_button_controller.h"
#include "chrome/browser/ui/performance_controls/tab_list_model.h"
#include "chrome/browser/ui/views/performance_controls/performance_intervention_button.h"
#include "chrome/browser/ui/views/performance_controls/tab_list_view.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/performance_manager/public/features.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {
const char kViewClassName[] = "PerformanceInterventionBubble";
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PerformanceInterventionBubble,
                                      kPerformanceInterventionDialogBody);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(
    PerformanceInterventionBubble,
    kPerformanceInterventionDialogDismissButton);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(
    PerformanceInterventionBubble,
    kPerformanceInterventionDialogDeactivateButton);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PerformanceInterventionBubble,
                                      kPerformanceInterventionTabList);

// static
views::BubbleDialogModelHost* PerformanceInterventionBubble::CreateBubble(
    views::BubbleAnchor anchor,
    PerformanceInterventionButtonController* button_controller) {
  auto tab_list_model_unique =
      std::make_unique<TabListModel>(button_controller->actionable_cpu_tabs());
  TabListModel* const tab_list_model = tab_list_model_unique.get();

  RecordSuggestedTabShownCount(tab_list_model->count());
  auto bubble_delegate =
      std::make_unique<PerformanceInterventionBubbleDelegate>(
          std::move(tab_list_model_unique), button_controller);

  const DialogStrings strings = GetStrings(tab_list_model->count());
  PerformanceInterventionBubbleDelegate* const delegate = bubble_delegate.get();
  auto dialog_model =
      ui::DialogModel::Builder(std::move(bubble_delegate))
          .SetInternalName(kViewClassName)
          .SetTitle(strings.title)
          .SetIsAlertDialog()
          .SetCloseActionCallback(base::BindOnce(
              &PerformanceInterventionBubbleDelegate::OnBubbleClosed,
              base::Unretained(delegate)))
          .AddParagraph(ui::DialogModelLabel(strings.body_text)
                            .set_is_secondary()
                            .set_allow_character_break(),
                        std::u16string(), kPerformanceInterventionDialogBody)
          .AddCustomField(
              std::make_unique<views::BubbleDialogModelHost::CustomView>(
                  std::make_unique<TabListView>(tab_list_model),
                  views::BubbleDialogModelHost::FieldType::kMenuItem),
              kPerformanceInterventionTabList)
          .AddOkButton(
              base::BindOnce(&PerformanceInterventionBubbleDelegate::
                                 OnDeactivateButtonClicked,
                             base::Unretained(delegate)),
              ui::DialogModel::Button::Params()
                  .SetLabel(strings.deactivate_tabs_button)
                  .SetId(kPerformanceInterventionDialogDeactivateButton))
          .AddCancelButton(
              base::BindOnce(&PerformanceInterventionBubbleDelegate::
                                 OnDismissButtonClicked,
                             base::Unretained(delegate)),
              ui::DialogModel::Button::Params()
                  .SetLabel(l10n_util::GetStringUTF16(
                      IDS_PERFORMANCE_INTERVENTION_DISMISS_BUTTON))
                  .SetId(kPerformanceInterventionDialogDismissButton))
          .Build();

  auto bubble_unique = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), anchor, views::BubbleBorder::TOP_RIGHT);
  auto* const bubble = bubble_unique.get();

  views::Widget* widget = views::BubbleDialogDelegate::CreateBubbleDeprecated(
      std::move(bubble_unique),
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  widget->widget_delegate()->SetEnableArrowKeyTraversal(true);
  widget->Show();
  button_controller->OnBubbleShown();

  return bubble;
}

// static
void PerformanceInterventionBubble::CloseBubble(
    views::BubbleDialogModelHost* bubble_dialog) {
  CHECK(bubble_dialog);
  if (bubble_dialog->GetWidget() && !bubble_dialog->GetWidget()->IsClosed()) {
    bubble_dialog->Close();
  }
}

// static
void PerformanceInterventionBubble::RecordCloseReason(
    views::Widget::ClosedReason closed_reason) {
  InterventionBubbleActionType action_type =
      InterventionBubbleActionType::kUnknown;
  switch (closed_reason) {
    case views::Widget::ClosedReason::kUnspecified:
      action_type = InterventionBubbleActionType::kUnknown;
      break;
    case views::Widget::ClosedReason::kEscKeyPressed:
    case views::Widget::ClosedReason::kCloseButtonClicked:
      action_type = InterventionBubbleActionType::kClose;
      break;
    case views::Widget::ClosedReason::kLostFocus:
      action_type = InterventionBubbleActionType::kIgnore;
      break;
    case views::Widget::ClosedReason::kCancelButtonClicked:
      action_type = InterventionBubbleActionType::kDismiss;
      break;
    case views::Widget::ClosedReason::kAcceptButtonClicked:
      action_type = InterventionBubbleActionType::kAccept;
      break;
  }

  RecordInterventionBubbleClosedReason(
      performance_manager::user_tuning::PerformanceDetectionManager::
          ResourceType::kCpu,
      action_type);
}

DialogStrings PerformanceInterventionBubble::GetStrings(int count) {
  const std::u16string body_text = l10n_util::GetStringUTF16(
      count > 1 ? IDS_PERFORMANCE_INTERVENTION_DIALOG_BODY_V1
                : IDS_PERFORMANCE_INTERVENTION_DIALOG_BODY_SINGULAR_V1);

  if (base::FeatureList::IsEnabled(
          performance_manager::features::
              kPerformanceInterventionNotificationStringImprovements)) {
    const int string_version =
        performance_manager::features::kNotificationStringVersion.Get();
    if (string_version == 1) {
      return {
          l10n_util::GetPluralStringFUTF16(
              IDS_PERFORMANCE_INTERVENTION_DIALOG_TITLE_UPDATED_V1, count),
          body_text,
          l10n_util::GetPluralStringFUTF16(
              IDS_PERFORMANCE_INTERVENTION_DEACTIVATE_TABS_BUTTON_UPDATED_V1,
              count)};
    } else if (string_version == 2) {
      return {
          l10n_util::GetPluralStringFUTF16(
              IDS_PERFORMANCE_INTERVENTION_DIALOG_TITLE_UPDATED_V2, count),
          body_text,
          l10n_util::GetPluralStringFUTF16(
              IDS_PERFORMANCE_INTERVENTION_DEACTIVATE_TABS_BUTTON_UPDATED_V2,
              count)};
    } else {
      return {
          l10n_util::GetPluralStringFUTF16(
              IDS_PERFORMANCE_INTERVENTION_DIALOG_TITLE_UPDATED_V3, count),
          body_text,
          l10n_util::GetPluralStringFUTF16(
              IDS_PERFORMANCE_INTERVENTION_DEACTIVATE_TABS_BUTTON_UPDATED_V2,
              count)};
    }
  } else {
    return {
        l10n_util::GetStringUTF16(IDS_PERFORMANCE_INTERVENTION_DIALOG_TITLE_V1),
        body_text,
        l10n_util::GetStringUTF16(
            IDS_PERFORMANCE_INTERVENTION_DEACTIVATE_TABS_BUTTON_V1)};
  }
}
