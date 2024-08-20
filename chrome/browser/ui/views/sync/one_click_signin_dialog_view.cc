// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/one_click_signin_dialog_view.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/google/core/common/google_util.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
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

  // Minimum width for the multi-line label.
  constexpr int kMinimumDialogLabelWidth = 400;
  views::Builder<OneClickSigninDialogView>(this)
      .SetButtonLabel(
          ui::mojom::DialogButton::kOk,
          l10n_util::GetStringUTF16(IDS_ONE_CLICK_SIGNIN_DIALOG_OK_BUTTON))
      .SetButtonLabel(
          ui::mojom::DialogButton::kCancel,
          l10n_util::GetStringUTF16(IDS_ONE_CLICK_SIGNIN_DIALOG_UNDO_BUTTON))
      .SetModalType(ui::mojom::ModalType::kWindow)
      .SetTitle(IDS_ONE_CLICK_SIGNIN_DIALOG_TITLE_NEW)
      .set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
          views::DialogContentType::kText, views::DialogContentType::kText))
      .SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical))
      .AddChildren(views::Builder<views::Label>()
                       .SetText(l10n_util::GetStringFUTF16(
                           IDS_ONE_CLICK_SIGNIN_DIALOG_MESSAGE_NEW, email_))
                       .SetMultiLine(true)
                       .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                       .CustomConfigure(base::BindOnce([](views::Label* label) {
                         label->SizeToFit(kMinimumDialogLabelWidth);
                       })),
                   views::Builder<views::Link>()
                       .SetText(l10n_util::GetStringUTF16(IDS_LEARN_MORE))
                       .SetCallback(base::BindRepeating(
                           &OneClickSigninLinksDelegate::OnLearnMoreLinkClicked,
                           std::move(delegate), true))
                       .SetHorizontalAlignment(gfx::ALIGN_LEFT))
      .SetExtraView(views::Builder<views::Link>()
                        .SetText(l10n_util::GetStringUTF16(
                            IDS_ONE_CLICK_SIGNIN_DIALOG_ADVANCED))
                        .SetCallback(base::BindRepeating(
                            [](OneClickSigninDialogView* view) {
                              if (view->Accept())
                                Hide();
                            },
                            base::Unretained(this)))
                        .SetHorizontalAlignment(gfx::ALIGN_LEFT))
      .BuildChildren();
}

OneClickSigninDialogView::~OneClickSigninDialogView() {
  if (!confirmed_callback_.is_null())
    std::move(confirmed_callback_).Run(false);
}

BEGIN_METADATA(OneClickSigninDialogView)
END_METADATA
