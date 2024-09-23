// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/memory_saver_bubble_view.h"

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/performance_controls/memory_saver_bubble_delegate.h"
#include "chrome/browser/ui/performance_controls/memory_saver_bubble_observer.h"
#include "chrome/browser/ui/performance_controls/memory_saver_chip_tab_helper.h"
#include "chrome/browser/ui/performance_controls/memory_saver_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/performance_controls/memory_saver_resource_view.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
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

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(MemorySaverBubbleView,
                                      kMemorySaverDialogBodyElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(MemorySaverBubbleView,
                                      kMemorySaverDialogResourceViewElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(MemorySaverBubbleView,
                                      kMemorySaverDialogOkButton);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(MemorySaverBubbleView,
                                      kMemorySaverDialogCancelButton);

namespace {
// The lower limit of memory usage that we would display to the user in bytes.
// This value is the equivalent of 10MB.
constexpr int64_t kMemoryUsageThresholdInBytes = 10 * 1024 * 1024;

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
      MemorySaverBubbleView::kMemorySaverDialogBodyElementId);
}

void AddCancelButton(ui::DialogModel::Builder* dialog_model_builder,
                     MemorySaverBubbleDelegate* bubble_delegate,
                     const bool is_site_excluded) {
  int button_string_id;
  base::OnceClosure callback;
  if (is_site_excluded) {
    button_string_id = IDS_MEMORY_SAVER_DIALOG_SETTINGS_BUTTON;
    callback = base::BindOnce(&MemorySaverBubbleDelegate::OnSettingsClicked,
                              base::Unretained(bubble_delegate));
  } else {
    button_string_id = IDS_MEMORY_SAVER_DIALOG_BUTTON_ADD_TO_EXCLUSION_LIST;
    callback = base::BindOnce(
        &MemorySaverBubbleDelegate::OnAddSiteToTabDiscardExceptionsListClicked,
        base::Unretained(bubble_delegate));
  }
  dialog_model_builder->AddCancelButton(
      std::move(callback),
      ui::DialogModel::Button::Params()
          .SetLabel(l10n_util::GetStringUTF16(button_string_id))
          .SetId(MemorySaverBubbleView::kMemorySaverDialogCancelButton));
}
}  // namespace

// static
views::BubbleDialogModelHost* MemorySaverBubbleView::ShowBubble(
    Browser* browser,
    views::View* anchor_view,
    MemorySaverBubbleObserver* observer) {
  auto bubble_delegate_unique =
      std::make_unique<MemorySaverBubbleDelegate>(browser, observer);
  auto* bubble_delegate = bubble_delegate_unique.get();
  auto dialog_model_builder =
      ui::DialogModel::Builder(std::move(bubble_delegate_unique));

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  dialog_model_builder
      .SetTitle(l10n_util::GetStringUTF16(IDS_MEMORY_SAVER_DIALOG_TITLE))
      .SetDialogDestroyingCallback(
          base::BindOnce(&MemorySaverBubbleDelegate::OnDialogDestroy,
                         base::Unretained(bubble_delegate)))
      .AddOkButton(base::DoNothing(),
                   ui::DialogModel::Button::Params()
                       .SetLabel(l10n_util::GetStringUTF16(IDS_OK))
                       .SetId(kMemorySaverDialogOkButton));

  const uint64_t memory_savings =
      memory_saver::GetDiscardedMemorySavingsInBytes(web_contents);

  ui::DialogModelLabel::TextReplacement memory_savings_text =
      ui::DialogModelLabel::CreatePlainText(ui::FormatBytes(memory_savings));

  Profile* const profile = browser->profile();
  const bool is_guest = profile->IsGuestSession();

  if (memory_savings > kMemoryUsageThresholdInBytes) {
    dialog_model_builder.AddCustomField(
        std::make_unique<views::BubbleDialogModelHost::CustomView>(
            std::make_unique<MemorySaverResourceView>(memory_savings),
            views::BubbleDialogModelHost::FieldType::kText),
        kMemorySaverDialogResourceViewElementId);
  }

  AddBubbleBodyText(&dialog_model_builder, IDS_MEMORY_SAVER_DIALOG_BODY);

  if (!is_guest && !profile->IsIncognitoProfile()) {
    dialog_model_builder.SetSubtitle(
        base::UTF8ToUTF16(web_contents->GetURL().host()));
    const bool is_site_excluded = performance_manager::user_tuning::prefs::
        IsSiteInTabDiscardExceptionsList(profile->GetPrefs(),
                                         web_contents->GetURL().host());
    AddCancelButton(&dialog_model_builder, bubble_delegate, is_site_excluded);
  }

  auto dialog_model = dialog_model_builder.Build();

  auto bubble_unique = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), anchor_view, views::BubbleBorder::TOP_RIGHT);
  auto* bubble = bubble_unique.get();
  bubble->SetHighlightedButton(
      BrowserView::GetBrowserViewForBrowser(browser)
          ->toolbar_button_provider()
          ->GetPageActionIconView(PageActionIconType::kMemorySaver));

  views::Widget* const widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(bubble_unique));
  widget->Show();
  observer->OnBubbleShown();
  return bubble;
}
