// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_in_progress_dialog_view.h"

#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

// static
void DownloadInProgressDialogView::Show(
    gfx::NativeWindow parent,
    int download_count,
    Browser::DownloadCloseType dialog_type,
    const base::Callback<void(bool)>& callback) {
  DownloadInProgressDialogView* window =
      new DownloadInProgressDialogView(download_count, dialog_type, callback);
  constrained_window::CreateBrowserModalDialogViews(window, parent)->Show();
}

DownloadInProgressDialogView::DownloadInProgressDialogView(
    int download_count,
    Browser::DownloadCloseType dialog_type,
    const base::Callback<void(bool)>& callback)
    : callback_(callback) {
  SetTitle(l10n_util::GetPluralStringFUTF16(IDS_ABANDON_DOWNLOAD_DIALOG_TITLE,
                                            download_count));
  SetShowCloseButton(false);
  SetModalType(ui::MODAL_TYPE_WINDOW);
  SetDefaultButton(ui::DIALOG_BUTTON_CANCEL);
  SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_ABANDON_DOWNLOAD_DIALOG_EXIT_BUTTON));
  SetButtonLabel(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(IDS_ABANDON_DOWNLOAD_DIALOG_CONTINUE_BUTTON));
  SetLayoutManager(std::make_unique<views::FillLayout>());
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::TEXT));

  auto run_callback = [](DownloadInProgressDialogView* dialog, bool accept) {
    // Note that accepting this dialog means "cancel the download", while cancel
    // means "continue the download".
    dialog->callback_.Run(accept);
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
      break;
  }
  auto message_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(message_id),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY);
  message_label->SetMultiLine(true);
  message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(message_label.release());

  chrome::RecordDialogCreation(chrome::DialogIdentifier::DOWNLOAD_IN_PROGRESS);
}

DownloadInProgressDialogView::~DownloadInProgressDialogView() = default;

gfx::Size DownloadInProgressDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

BEGIN_METADATA(DownloadInProgressDialogView, views::DialogDelegateView)
END_METADATA
