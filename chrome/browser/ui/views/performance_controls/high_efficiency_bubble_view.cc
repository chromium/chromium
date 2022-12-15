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
#include "chrome/browser/ui/performance_controls/tab_discard_tab_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/google_chrome_strings.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/interaction/element_tracker_views.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(HighEfficiencyBubbleView,
                                      kHighEfficiencyDialogBodyElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(HighEfficiencyBubbleView,
                                      kHighEfficiencyDialogOkButton);

namespace {
// The lower limit of memory usage that we would display to the user in bytes.
// This value is the equivalent of 10MB.
constexpr uint64_t kMemoryUsageThresholdInBytes = 10 * 1024 * 1024;
}  // namespace

// static
views::BubbleDialogModelHost* HighEfficiencyBubbleView::ShowBubble(
    Browser* browser,
    views::View* anchor_view,
    HighEfficiencyBubbleObserver* observer) {
  auto bubble_delegate_unique =
      std::make_unique<HighEfficiencyBubbleDelegate>(browser, observer);
  auto* bubble_delegate = bubble_delegate_unique.get();

  auto dialog_model_builder =
      ui::DialogModel::Builder(std::move(bubble_delegate_unique));
  dialog_model_builder
      .SetTitle(l10n_util::GetStringUTF16(IDS_HIGH_EFFICIENCY_DIALOG_TITLE))
      .SetDialogDestroyingCallback(
          base::BindOnce(&HighEfficiencyBubbleDelegate::OnDialogDestroy,
                         base::Unretained(bubble_delegate)))
      .AddOkButton(base::DoNothing(),
                   ui::DialogModelButton::Params()
                       .SetLabel(l10n_util::GetStringUTF16(IDS_OK))
                       .SetId(kHighEfficiencyDialogOkButton));

  TabDiscardTabHelper* const tab_helper = TabDiscardTabHelper::FromWebContents(
      browser->tab_strip_model()->GetActiveWebContents());
  const uint64_t memory_savings = tab_helper->GetMemorySavingsInBytes();

  if (memory_savings > kMemoryUsageThresholdInBytes) {
    dialog_model_builder.AddParagraph(
        ui::DialogModelLabel::CreateWithReplacements(
            IDS_HIGH_EFFICIENCY_DIALOG_BODY_WITH_SAVINGS_AND_LINK,
            {ui::DialogModelLabel::CreatePlainText(
                 ui::FormatBytes(memory_savings)),
             ui::DialogModelLabel::CreateLink(
                 IDS_HIGH_EFFICIENCY_DIALOG_BODY_LINK_TEXT,
                 base::BindRepeating(
                     &HighEfficiencyBubbleDelegate::OnSettingsClicked,
                     base::Unretained(bubble_delegate)))})
            .set_is_secondary(),
        std::u16string(), kHighEfficiencyDialogBodyElementId);
  } else {
    dialog_model_builder.AddParagraph(
        ui::DialogModelLabel::CreateWithReplacement(
            IDS_HIGH_EFFICIENCY_DIALOG_BODY,
            ui::DialogModelLabel::CreateLink(
                IDS_HIGH_EFFICIENCY_DIALOG_BODY_LINK_TEXT,
                base::BindRepeating(
                    &HighEfficiencyBubbleDelegate::OnSettingsClicked,
                    base::Unretained(bubble_delegate))))
            .set_is_secondary(),
        std::u16string(), kHighEfficiencyDialogBodyElementId);
  }
  auto dialog_model = dialog_model_builder.Build();

  auto bubble_unique = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), anchor_view, views::BubbleBorder::TOP_RIGHT);
  auto* bubble = bubble_unique.get();
  bubble->SetHighlightedButton(
      BrowserView::GetBrowserViewForBrowser(browser)
          ->toolbar_button_provider()
          ->GetPageActionIconView(PageActionIconType::kHighEfficiency));

  views::Widget* const widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(bubble_unique));
  widget->Show();
  observer->OnBubbleShown();
  return bubble;
}
