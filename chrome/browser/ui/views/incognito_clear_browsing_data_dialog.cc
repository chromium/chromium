// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/incognito_clear_browsing_data_dialog.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace {
IncognitoClearBrowsingDataDialog* g_incognito_cbd_dialog = nullptr;
}  // namespace

// static
void IncognitoClearBrowsingDataDialog::Show(views::View* anchor_view,
                                            Profile* incognito_profile) {
  g_incognito_cbd_dialog =
      new IncognitoClearBrowsingDataDialog(anchor_view, incognito_profile);
  views::Widget* const widget =
      BubbleDialogDelegateView::CreateBubble(g_incognito_cbd_dialog);
  widget->Show();
}

// static
bool IncognitoClearBrowsingDataDialog::IsShowing() {
  return g_incognito_cbd_dialog != nullptr;
}

// static
void IncognitoClearBrowsingDataDialog::CloseDialog() {
  if (IsShowing())
    g_incognito_cbd_dialog->GetWidget()->Close();
}

// static
IncognitoClearBrowsingDataDialog* IncognitoClearBrowsingDataDialog::
    GetIncognitoClearBrowsingDataDialogForTesting() {
  return g_incognito_cbd_dialog;
}

void IncognitoClearBrowsingDataDialog::SetDestructorCallbackForTesting(
    base::OnceClosure callback) {
  destructor_callback_ = std::move(callback);
}

IncognitoClearBrowsingDataDialog::~IncognitoClearBrowsingDataDialog() {
  g_incognito_cbd_dialog = nullptr;
  std::move(destructor_callback_).Run();
}

IncognitoClearBrowsingDataDialog::IncognitoClearBrowsingDataDialog(
    views::View* anchor_view,
    Profile* incognito_profile)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      incognito_profile_(incognito_profile) {
  DCHECK(incognito_profile_);
  DCHECK(incognito_profile_->IsIncognitoProfile());
  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetShowCloseButton(true);
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));

  AddChildView(views::Builder<views::Label>()
                   .SetText(l10n_util::GetStringUTF16(
                       IDS_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_PRIMARY_TEXT))
                   .SetFontList(views::style::GetFont(
                       views::style::CONTEXT_LABEL, STYLE_EMPHASIZED))
                   .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                   .Build());

  AddChildView(
      views::Builder<views::Label>()
          .SetText(l10n_util::GetStringUTF16(
              IDS_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_SECONDARY_TEXT))
          .SetFontList(views::style::GetFont(views::style::CONTEXT_LABEL,
                                             views::style::STYLE_SECONDARY))
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .Build());

  SetButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
  SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(
          IDS_INCOGNITO_CLEAR_BROWSING_DATA_DIALOG_CLOSE_WINDOWS_BUTTON));

  SetAcceptCallback(base::BindOnce(
      &IncognitoClearBrowsingDataDialog::OnCloseWindowsButtonClicked,
      base::Unretained(this)));
  SetCancelCallback(
      base::BindOnce(&IncognitoClearBrowsingDataDialog::OnCancelButtonClicked,
                     base::Unretained(this)));
}

void IncognitoClearBrowsingDataDialog::OnCloseWindowsButtonClicked() {
  // Skipping before-unload trigger to give incognito mode users a chance to
  // quickly close all incognito windows without needing to confirm closing the
  // open forms.
  BrowserList::CloseAllBrowsersWithIncognitoProfile(
      incognito_profile_, base::DoNothing(), base::DoNothing(),
      /*skip_beforeunload=*/true);
}

void IncognitoClearBrowsingDataDialog::OnCancelButtonClicked() {
  CloseDialog();
}

BEGIN_METADATA(IncognitoClearBrowsingDataDialog,
               views::BubbleDialogDelegateView)
END_METADATA
