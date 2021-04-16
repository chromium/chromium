// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/chrome_labs_item_view.h"
#include "build/build_config.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_bubble_view_model.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/api/feedback_private/feedback_private_api.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace {

void ShowFeedbackPage(Browser* browser,
                      std::string feedback_category_name,
                      std::u16string visible_name) {
  chrome::ShowFeedbackPage(
      browser, chrome::FeedbackSource::kFeedbackSourceChromeLabs,
      /*description_template=*/std::string(),
      /*description_placeholder_text=*/
      l10n_util::GetStringFUTF8(
          IDS_CHROMELABS_SEND_FEEDBACK_DESCRIPTION_PLACEHOLDER,
          std::move(visible_name)),
      /*category_tag=*/std::move(feedback_category_name),
      /* extra_diagnostics=*/std::string());
}

}  // namespace

class LabsComboboxModel : public ui::ComboboxModel {
 public:
  explicit LabsComboboxModel(const LabInfo& lab,
                             const flags_ui::FeatureEntry* feature_entry,
                             int default_index)
      : lab_(lab),
        feature_entry_(feature_entry),
        default_index_(default_index) {}

  // ui::ComboboxModel:
  int GetItemCount() const override { return feature_entry_->NumOptions(); }

  // The order in which these descriptions are returned is the same in
  // flags_ui::FeatureEntry::DescriptionForOption(..). If there are changes to
  // this, the same changes must be made in
  // flags_ui::FeatureEntry::DescriptionForOption(..).
  std::u16string GetItemAt(int index) const override {
    DCHECK_LT(index, feature_entry_->NumOptions());
    int description_translation_id = IDS_CHROMELABS_DEFAULT;
    if (feature_entry_->type ==
        flags_ui::FeatureEntry::FEATURE_WITH_PARAMS_VALUE) {
      if (index == 0) {
        description_translation_id = IDS_CHROMELABS_DEFAULT;
      } else if (index == 1) {
        description_translation_id = IDS_CHROMELABS_ENABLED;
      } else if (index < feature_entry_->NumOptions() - 1) {
        // First two options do not have variations params.
        int variation_index = index - 2;
        return l10n_util::GetStringFUTF16(
            IDS_CHROMELABS_ENABLED_WITH_VARIATION_NAME,
            lab_.translated_feature_variation_descriptions[variation_index]);
      } else {
        DCHECK_EQ(feature_entry_->NumOptions() - 1, index);
        description_translation_id = IDS_CHROMELABS_DISABLED;
      }
    } else {
      const int kEnableDisableDescriptions[] = {
          IDS_CHROMELABS_DEFAULT,
          IDS_CHROMELABS_ENABLED,
          IDS_CHROMELABS_DISABLED,
      };
      description_translation_id = kEnableDisableDescriptions[index];
    }
    return l10n_util::GetStringUTF16(description_translation_id);
  }

  int GetDefaultIndex() const override { return default_index_; }

 private:
  const LabInfo& lab_;
  const flags_ui::FeatureEntry* feature_entry_;
  int default_index_;
};

ChromeLabsItemView::ChromeLabsItemView(
    const LabInfo& lab,
    int default_index,
    const flags_ui::FeatureEntry* feature_entry,
    base::RepeatingCallback<void(ChromeLabsItemView* item_view)>
        combobox_callback,
    Browser* browser)
    : feature_entry_(feature_entry) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets(ChromeLayoutProvider::Get()->GetDistanceMetric(
                      DISTANCE_CONTROL_LIST_VERTICAL),
                  0)));

  views::Label* experiment_name;
  views::Label* experiment_description;
  AddChildView(views::Builder<views::Label>()
                   .CopyAddressTo(&experiment_name)
                   .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
                   .SetText(lab.visible_name)
                   .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                   .Build());
  AddChildView(
      views::Builder<views::Label>()
          .CopyAddressTo(&experiment_description)
          .SetText(lab.visible_description)
          .SetTextContext(ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL)
          .SetTextStyle(views::style::STYLE_SECONDARY)
          .SetMultiLine(true)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetProperty(views::kFlexBehaviorKey,
                       views::FlexSpecification(
                           views::MinimumFlexSizeRule::kPreferred,
                           views::MaximumFlexSizeRule::kPreferred, true))
          .SetBorder(views::CreateEmptyBorder(
              gfx::Insets(0, 0,
                          views::LayoutProvider::Get()->GetDistanceMetric(
                              views::DISTANCE_RELATED_CONTROL_VERTICAL),
                          0)))
          .Build());

  // It may cause confusion if screen readers read out all experiments and
  // descriptions when the bubble first opens. Experiment name and description
  // will be read out when a user enters the grouping.
  // See crbug.com/1145666 Accessibility review.
  experiment_name->GetViewAccessibility().OverrideIsIgnored(true);
  experiment_description->GetViewAccessibility().OverrideIsIgnored(true);
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kGroup);
  GetViewAccessibility().OverrideName(lab.visible_name);

  // There is currently a MacOS VoiceOver screen reader bug where VoiceOver does
  // not announce the accessible description for groups (crbug.com/1197159). The
  // MacOS specific code here provides a temporary mitigation for screen reader
  // users and moves announcing the description to when the user interacts with
  // the combobox of that experiment. Don’t add an accessible description for
  // now to prevent the screen reader from announcing the description twice in
  // the time between when the VoiceOver bug is fixed and this code gets
  // removed.
  // TODO(elainechien): Remove MacOS specific code for experiment description
  // when VoiceOver bug is fixed.

#if !defined(OS_MAC)
  GetViewAccessibility().OverrideDescription(lab.visible_description);
#endif

  AddChildView(
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .AddChildren({
            views::Builder<views::Combobox>()
                .CopyAddressTo(&lab_state_combobox_)
                .SetTooltipTextAndAccessibleName(l10n_util::GetStringFUTF16(
                    IDS_TOOLTIP_CHROMELABS_COMBOBOX, lab.visible_name))
#if defined(OS_MAC)
                .SetAccessibleName(l10n_util::GetStringFUTF16(
                    IDS_ACCNAME_CHROMELABS_COMBOBOX_MAC, lab.visible_name,
                    lab.visible_description))
#else
                .SetAccessibleName(l10n_util::GetStringFUTF16(
                    IDS_ACCNAME_CHROMELABS_COMBOBOX, lab.visible_name))

#endif
                .SetOwnedModel(std::make_unique<LabsComboboxModel>(
                    lab, feature_entry_, default_index))
                .SetCallback(base::BindRepeating(combobox_callback, this))

                .SetProperty(views::kFlexBehaviorKey,
                             views::FlexSpecification(
                                 views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kPreferred))

                .SetSizeToLargestLabel(false),
                views::Builder<views::MdTextButton>()
                    .CopyAddressTo(&feedback_button_)
                    .SetTooltipText(l10n_util::GetStringFUTF16(
                        IDS_TOOLTIP_CHROMELABS_FEEDBACK_BUTTON,
                        lab.visible_name))
                    .SetCallback(base::BindRepeating(&ShowFeedbackPage, browser,
                                                     lab.feedback_category_name,
                                                     lab.visible_name))
                    .SetText(
                        l10n_util::GetStringUTF16(IDS_CHROMELABS_SEND_FEEDBACK))
                    .SetProperty(
                        views::kMarginsKey,
                        gfx::Insets(
                            0,
                            views::LayoutProvider::Get()->GetDistanceMetric(
                                views::DISTANCE_RELATED_CONTROL_HORIZONTAL),
                            0, 0))
                    .SetProperty(
                        views::kFlexBehaviorKey,
                        views::FlexSpecification(
                            views::MinimumFlexSizeRule::kPreferred,
                            views::MaximumFlexSizeRule::kUnbounded)
                            .WithAlignment(views::LayoutAlignment::kEnd))
          })
          .Build());
}

int ChromeLabsItemView::GetSelectedIndex() const {
  return lab_state_combobox_->GetSelectedIndex();
}

const flags_ui::FeatureEntry* ChromeLabsItemView::GetFeatureEntry() {
  return feature_entry_;
}

BEGIN_METADATA(ChromeLabsItemView, views::View)
ADD_READONLY_PROPERTY_METADATA(int, SelectedIndex)
END_METADATA
