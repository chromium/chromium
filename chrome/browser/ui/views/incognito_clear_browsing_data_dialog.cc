// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/incognito_clear_browsing_data_dialog.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"

IncognitoClearBrowsingDataDialog::IncognitoClearBrowsingDataDialog(
    views::View* anchor_view,
    Profile* incognito_profile,
    Type type)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      dialog_type_(type),
      incognito_profile_(incognito_profile) {
  DCHECK(incognito_profile_);
  DCHECK(incognito_profile_->IsIncognitoProfile());
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetShowCloseButton(true);

  // Layout
  int vertical_spacing = views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);
  views::FlexLayout* layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetDefault(views::kMarginsKey, gfx::Insets::VH(vertical_spacing, 0));
  layout->SetCollapseMargins(true);
  layout->SetIgnoreDefaultMainAxisMargins(true);

  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));

  // Header art
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  auto image_view = std::make_unique<ThemeTrackingNonAccessibleImageView>(
      *bundle.GetImageSkiaNamed(IDR_INCOGNITO_DATA_NOT_SAVED_HEADER_LIGHT),
      *bundle.GetImageSkiaNamed(IDR_INCOGNITO_DATA_NOT_SAVED_HEADER_DARK),
      base::BindRepeating(&views::BubbleDialogDelegate::GetBackgroundColor,
                          base::Unretained(this)));
  AddChildView(std::move(image_view));

  // Set bubble regarding to the type.
  if (type == kHistoryDisclaimerBubble)
    SetDialogForHistoryDisclaimerBubbleType();
  else
    SetDialogForDefaultBubbleType();
}

void IncognitoClearBrowsingDataDialog::SetDialogForDefaultBubbleType() {
  // Text
  const auto& typography_provider = views::TypographyProvider::Get();
  AddChildView(
      views::Builder<views::Label>()
          .SetText(l10n_util::GetStringUTF16(
              IDS_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_PRIMARY_TEXT))
          .SetFontList(typography_provider.GetFont(
              views::style::CONTEXT_LABEL, views::style::STYLE_EMPHASIZED))
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .Build());

  AddChildView(
      views::Builder<views::Label>()
          .SetText(l10n_util::GetStringUTF16(
              IDS_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_SECONDARY_TEXT))
          .SetFontList(typography_provider.GetFont(
              views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY))
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .Build());

  // Buttons
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
             static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      l10n_util::GetStringUTF16(
          IDS_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_CLOSE_WINDOWS_BUTTON));

  SetAcceptCallback(base::BindOnce(
      &IncognitoClearBrowsingDataDialog::OnCloseWindowsButtonClicked,
      base::Unretained(this)));
  SetCancelCallback(
      base::BindOnce(&IncognitoClearBrowsingDataDialog::OnCancelButtonClicked,
                     base::Unretained(this)));
}

void IncognitoClearBrowsingDataDialog::
    SetDialogForHistoryDisclaimerBubbleType() {
  // Text
  const auto& typography_provider = views::TypographyProvider::Get();
  AddChildView(
      views::Builder<views::Label>()
          .SetText(l10n_util::GetStringUTF16(
              IDS_INCOGNITO_HISTORY_BUBBLE_PRIMARY_TEXT))
          .SetFontList(typography_provider.GetFont(
              views::style::CONTEXT_LABEL, views::style::STYLE_EMPHASIZED))
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .Build());

  views::Label* label = AddChildView(
      views::Builder<views::Label>()
          .SetText(l10n_util::GetStringUTF16(
              IDS_INCOGNITO_HISTORY_BUBBLE_SECONDARY_TEXT))
          .SetFontList(typography_provider.GetFont(
              views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY))
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetMultiLine(true)
          .Build());
  label->SizeToFit(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));

  // Buttons
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
             static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(
                     IDS_INCOGNITO_HISTORY_BUBBLE_CANCEL_BUTTON_TEXT));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 l10n_util::GetStringUTF16(
                     IDS_INCOGNITO_HISTORY_BUBBLE_CLOSE_INCOGNITO_BUTTON_TEXT));

  SetAcceptCallback(
      base::BindOnce(&IncognitoClearBrowsingDataDialog::OnCancelButtonClicked,
                     base::Unretained(this)));
  SetCancelCallback(base::BindOnce(
      &IncognitoClearBrowsingDataDialog::OnCloseWindowsButtonClicked,
      base::Unretained(this)));
}

void IncognitoClearBrowsingDataDialog::OnCloseWindowsButtonClicked() {
  if (dialog_type_ == Type::kDefaultBubble) {
    base::UmaHistogramEnumeration(
        "Incognito.ClearBrowsingDataDialog.ActionType",
        DialogActionType::kCloseIncognito);
  }

  // Skipping before-unload trigger to give incognito mode users a chance to
  // quickly close all incognito windows without needing to confirm closing the
  // open forms.
  BrowserList::CloseAllBrowsersWithIncognitoProfile(
      incognito_profile_, base::DoNothing(), base::DoNothing(),
      /*skip_beforeunload=*/true);
}

void IncognitoClearBrowsingDataDialog::OnCancelButtonClicked() {
  if (dialog_type_ == Type::kDefaultBubble) {
    base::UmaHistogramEnumeration(
        "Incognito.ClearBrowsingDataDialog.ActionType",
        DialogActionType::kCancel);
  }

  GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
}

BEGIN_METADATA(IncognitoClearBrowsingDataDialog)
END_METADATA
