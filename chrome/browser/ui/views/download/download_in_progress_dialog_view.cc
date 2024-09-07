// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_in_progress_dialog_view.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

// static
void DownloadInProgressDialogView::Show(
    gfx::NativeWindow parent,
    int download_count,
    Browser::DownloadCloseType dialog_type,
    base::OnceCallback<void(bool)> callback) {
  DownloadInProgressDialogView* window = new DownloadInProgressDialogView(
      download_count, dialog_type, std::move(callback));
  constrained_window::CreateBrowserModalDialogViews(window, parent)->Show();
}

DownloadInProgressDialogView::DownloadInProgressDialogView(
    int download_count,
    Browser::DownloadCloseType dialog_type,
    base::OnceCallback<void(bool)> callback)
    : callback_(std::move(callback)) {
  SetTitle(l10n_util::GetPluralStringFUTF16(IDS_ABANDON_DOWNLOAD_DIALOG_TITLE,
                                            download_count));
  SetShowCloseButton(false);
  SetModalType(ui::mojom::ModalType::kWindow);
  SetDefaultButton(static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      l10n_util::GetStringUTF16(IDS_ABANDON_DOWNLOAD_DIALOG_EXIT_BUTTON));
  SetButtonLabel(
      ui::mojom::DialogButton::kCancel,
      l10n_util::GetStringUTF16(IDS_ABANDON_DOWNLOAD_DIALOG_CONTINUE_BUTTON));
  SetLayoutManager(std::make_unique<views::FillLayout>());
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  auto run_callback = [](DownloadInProgressDialogView* dialog, bool accept) {
    // Note that accepting this dialog means "cancel the download", while cancel
    // means "continue the download".
    std::move(dialog->callback_).Run(accept);
  };
  SetAcceptCallback(base::BindOnce(run_callback, base::Unretained(this), true));
  SetCancelCallback(
      base::BindOnce(run_callback, base::Unretained(this), false));
  SetCloseCallback(base::BindOnce(run_callback, base::Unretained(this), false));

  int message_id = 0;
  switch (dialog_type) {
    case Browser::DownloadCloseType::kLastWindowInIncognitoProfile:
      message_id = IDS_ABANDON_DOWNLOAD_DIALOG_INCOGNITO_MESSAGE;
      break;
    case Browser::DownloadCloseType::kLastWindowInGuestSession:
      message_id = IDS_ABANDON_DOWNLOAD_DIALOG_GUEST_MESSAGE;
      break;
    case Browser::DownloadCloseType::kBrowserShutdown:
      message_id = IDS_ABANDON_DOWNLOAD_DIALOG_BROWSER_MESSAGE;
      break;
    case Browser::DownloadCloseType::kOk:
      // This dialog should have been created within the same thread invocation
      // as the original test, so it's never ok to close.
      NOTREACHED();
  }
  auto message_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(message_id),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY);
  message_label->SetMultiLine(true);
  message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(message_label.release());
}

DownloadInProgressDialogView::~DownloadInProgressDialogView() = default;

BEGIN_METADATA(DownloadInProgressDialogView)
END_METADATA
