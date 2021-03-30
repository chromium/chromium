// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/one_click_signin_dialog_view.h"

#include <utility>

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/google/core/common/google_util.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/widget/widget.h"

// static
OneClickSigninDialogView* OneClickSigninDialogView::dialog_view_ = nullptr;

// static
void OneClickSigninDialogView::ShowDialog(
    const std::u16string& email,
    std::unique_ptr<OneClickSigninLinksDelegate> delegate,
    gfx::NativeWindow window,
    base::OnceCallback<void(bool)> confirmed_callback) {
  if (IsShowing())
    return;

  dialog_view_ = new OneClickSigninDialogView(email, std::move(delegate),
                                              std::move(confirmed_callback));
  constrained_window::CreateBrowserModalDialogViews(dialog_view_, window)
      ->Show();
}

// static
bool OneClickSigninDialogView::IsShowing() {
  return dialog_view_ != nullptr;
}

// static
void OneClickSigninDialogView::Hide() {
  if (IsShowing())
    dialog_view_->GetWidget()->Close();
}

void OneClickSigninDialogView::WindowClosing() {
  // We have to reset |dialog_view_| here, not in our destructor, because
  // we'll be destroyed asynchronously and the shown state will be checked
  // before then.
  DCHECK_EQ(dialog_view_, this);
  dialog_view_ = nullptr;
}

bool OneClickSigninDialogView::Accept() {
  std::move(confirmed_callback_).Run(true);
  return true;
}

OneClickSigninDialogView::OneClickSigninDialogView(
    const std::u16string& email,
    std::unique_ptr<OneClickSigninLinksDelegate> delegate,
    base::OnceCallback<void(bool)> confirmed_callback)
    : email_(email), confirmed_callback_(std::move(confirmed_callback)) {
  DCHECK(!confirmed_callback_.is_null());

  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());

  // Column set for descriptive text and link.
  views::ColumnSet* cs = layout->AddColumnSet(0);
  cs->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1.0,
                views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  layout->StartRow(views::GridLayout::kFixedSize, 0);

  auto label = std::make_unique<views::Label>(l10n_util::GetStringFUTF16(
      IDS_ONE_CLICK_SIGNIN_DIALOG_MESSAGE_NEW, email_));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  // Minimum width for the multi-line label.
  constexpr int kMinimumDialogLabelWidth = 400;
  label->SizeToFit(kMinimumDialogLabelWidth);
  layout->AddView(std::move(label));

  layout->StartRow(views::GridLayout::kFixedSize, 0);

  auto learn_more_link =
      std::make_unique<views::Link>(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  learn_more_link->SetCallback(
      base::BindRepeating(&OneClickSigninLinksDelegate::OnLearnMoreLinkClicked,
                          std::move(delegate), true));
  learn_more_link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->AddView(std::move(learn_more_link), 1, 1, views::GridLayout::TRAILING,
                  views::GridLayout::CENTER);

  auto advanced_link = std::make_unique<views::Link>(
      l10n_util::GetStringUTF16(IDS_ONE_CLICK_SIGNIN_DIALOG_ADVANCED));
  advanced_link->SetCallback(base::BindRepeating(
      [](OneClickSigninDialogView* view) {
        if (view->Accept())
          Hide();
      },
      base::Unretained(this)));
  advanced_link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  SetExtraView(std::move(advanced_link));

  SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_ONE_CLICK_SIGNIN_DIALOG_OK_BUTTON));
  SetButtonLabel(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(IDS_ONE_CLICK_SIGNIN_DIALOG_UNDO_BUTTON));
  SetModalType(ui::MODAL_TYPE_WINDOW);
  SetTitle(IDS_ONE_CLICK_SIGNIN_DIALOG_TITLE_NEW);

  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::TEXT));
  chrome::RecordDialogCreation(chrome::DialogIdentifier::ONE_CLICK_SIGNIN);
}

OneClickSigninDialogView::~OneClickSigninDialogView() {
  if (!confirmed_callback_.is_null())
    std::move(confirmed_callback_).Run(false);
}

BEGIN_METADATA(OneClickSigninDialogView, views::DialogDelegateView)
END_METADATA
