// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/high_efficiency_bubble_view.h"

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/performance_controls/high_efficiency_bubble_delegate.h"
#include "chrome/browser/ui/performance_controls/high_efficiency_bubble_observer.h"
#include "chrome/browser/ui/performance_controls/high_efficiency_chip_tab_helper.h"
#include "chrome/browser/ui/performance_controls/high_efficiency_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/performance_controls/high_efficiency_resource_view.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/google_chrome_strings.h"
#include "components/performance_manager/public/features.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/interaction/element_tracker_views.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(HighEfficiencyBubbleView,
                                      kHighEfficiencyDialogBodyElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(
    HighEfficiencyBubbleView,
    kHighEfficiencyDialogResourceViewElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(HighEfficiencyBubbleView,
                                      kHighEfficiencyDialogOkButton);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(HighEfficiencyBubbleView,
                                      kHighEfficiencyDialogCancelButton);

namespace {
// The lower limit of memory usage that we would display to the user in bytes.
// This value is the equivalent of 10MB.
constexpr uint64_t kMemoryUsageThresholdInBytes = 10 * 1024 * 1024;

void AddBubbleBodyText(
    ui::DialogModel::Builder* dialog_model_builder,
    int text_id,
    std::vector<ui::DialogModelLabel::TextReplacement> replacements = {}) {
  ui::DialogModelLabel label =
      replacements.empty()
          ? ui::DialogModelLabel(text_id).set_is_secondary()
          : ui::DialogModelLabel::CreateWithReplacements(text_id, replacements)
                .set_is_secondary();

  dialog_model_builder->AddParagraph(
      label, std::u16string(),
      HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId);
}

void AddCancelButton(ui::DialogModel::Builder* dialog_model_builder,
                     HighEfficiencyBubbleDelegate* bubble_delegate,
                     const bool is_site_excluded) {
  int button_string_id;
  base::OnceClosure callback;
  if (is_site_excluded) {
    button_string_id = IDS_HIGH_EFFICIENCY_DIALOG_BODY_LINK_TEXT;
    callback = base::BindOnce(&HighEfficiencyBubbleDelegate::OnSettingsClicked,
                              base::Unretained(bubble_delegate));
  } else {
    button_string_id = IDS_HIGH_EFFICIENCY_DIALOG_BUTTON_ADD_TO_EXCLUSION_LIST;
    callback = base::BindOnce(
        &HighEfficiencyBubbleDelegate::OnAddSiteToExceptionsListClicked,
        base::Unretained(bubble_delegate));
  }
  dialog_model_builder->AddCancelButton(
      std::move(callback),
      ui::DialogModelButton::Params()
          .SetLabel(l10n_util::GetStringUTF16(button_string_id))
          .SetId(HighEfficiencyBubbleView::kHighEfficiencyDialogCancelButton));
}
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

  const bool show_memory_savings_chart = base::FeatureList::IsEnabled(
      performance_manager::features::kMemorySavingsReportingImprovements);

  dialog_model_builder
      .SetTitle(
          show_memory_savings_chart
              ? l10n_util::GetStringUTF16(IDS_HIGH_EFFICIENCY_DIALOG_TITLE_V2)
              : l10n_util::GetStringUTF16(IDS_HIGH_EFFICIENCY_DIALOG_TITLE))
      .SetDialogDestroyingCallback(
          base::BindOnce(&HighEfficiencyBubbleDelegate::OnDialogDestroy,
                         base::Unretained(bubble_delegate)))
      .AddOkButton(base::DoNothing(),
                   ui::DialogModelButton::Params()
                       .SetLabel(l10n_util::GetStringUTF16(IDS_OK))
                       .SetId(kHighEfficiencyDialogOkButton));

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  HighEfficiencyChipTabHelper* const tab_helper =
      HighEfficiencyChipTabHelper::FromWebContents(web_contents);
  const uint64_t memory_savings = tab_helper->GetMemorySavingsInBytes();

  ui::DialogModelLabel::TextReplacement memory_savings_text =
      ui::DialogModelLabel::CreatePlainText(ui::FormatBytes(memory_savings));

  Profile* const profile = browser->profile();
  const bool is_guest = profile->IsGuestSession();
  const bool is_forced_incognito =
      IncognitoModePrefs::GetAvailability(profile->GetPrefs()) ==
      policy::IncognitoModeAvailability::kForced;

  if (show_memory_savings_chart) {
    if (memory_savings > kMemoryUsageThresholdInBytes) {
      dialog_model_builder.AddCustomField(
          std::make_unique<views::BubbleDialogModelHost::CustomView>(
              std::make_unique<HighEfficiencyResourceView>(memory_savings),
              views::BubbleDialogModelHost::FieldType::kText),
          kHighEfficiencyDialogResourceViewElementId);
    }

    AddBubbleBodyText(&dialog_model_builder,
                      IDS_HIGH_EFFICIENCY_DIALOG_BODY_V2);
  } else if (is_guest || is_forced_incognito) {
    // Show bubble without Performance Settings Page Link since guest users or
    // forced incognito users are not allowed to navigate to the performance
    // settings page
    if (memory_savings > kMemoryUsageThresholdInBytes) {
      AddBubbleBodyText(&dialog_model_builder,
                        IDS_HIGH_EFFICIENCY_DIALOG_BODY_WITH_SAVINGS,
                        {memory_savings_text});
    } else {
      AddBubbleBodyText(&dialog_model_builder,
                        IDS_HIGH_EFFICIENCY_DIALOG_BODY_WITHOUT_LINK);
    }
  } else {
    ui::DialogModelLabel::TextReplacement settings_link =
        ui::DialogModelLabel::CreateLink(
            IDS_HIGH_EFFICIENCY_DIALOG_BODY_LINK_TEXT,
            base::BindRepeating(
                &HighEfficiencyBubbleDelegate::OnSettingsClicked,
                base::Unretained(bubble_delegate)));

    if (memory_savings > kMemoryUsageThresholdInBytes) {
      AddBubbleBodyText(&dialog_model_builder,
                        IDS_HIGH_EFFICIENCY_DIALOG_BODY_WITH_SAVINGS_AND_LINK,
                        {memory_savings_text, settings_link});
    } else {
      AddBubbleBodyText(&dialog_model_builder, IDS_HIGH_EFFICIENCY_DIALOG_BODY,
                        {settings_link});
    }
  }

  if (base::FeatureList::IsEnabled(
          performance_manager::features::kDiscardExceptionsImprovements) &&
      !is_guest && !profile->IsIncognitoProfile()) {
    const bool is_site_excluded = high_efficiency::IsSiteInExceptionsList(
        profile->GetPrefs(), web_contents->GetURL().host());
    AddCancelButton(&dialog_model_builder, bubble_delegate, is_site_excluded);
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
