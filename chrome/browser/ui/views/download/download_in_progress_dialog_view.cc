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
    bool app_modal,
    const base::Callback<void(bool)>& callback) {
  DownloadInProgressDialogView* window = new DownloadInProgressDialogView(
      download_count, dialog_type, app_modal, callback);
  constrained_window::CreateBrowserModalDialogViews(window, parent)->Show();
}

DownloadInProgressDialogView::DownloadInProgressDialogView(
    int download_count,
    Browser::DownloadCloseType dialog_type,
    bool app_modal,
    const base::Callback<void(bool)>& callback)
    : download_count_(download_count),
      app_modal_(app_modal),
      callback_(callback) {
  DialogDelegate::set_default_button(ui::DIALOG_BUTTON_CANCEL);
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_ABANDON_DOWNLOAD_DIALOG_EXIT_BUTTON));
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(IDS_ABANDON_DOWNLOAD_DIALOG_CONTINUE_BUTTON));
  SetLayoutManager(std::make_unique<views::FillLayout>());
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::TEXT));

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
      l10n_util::GetStringUTF16(message_id), CONTEXT_BODY_TEXT_LARGE,
      views::style::STYLE_SECONDARY);
  message_label->SetMultiLine(true);
  message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(message_label.release());

  chrome::RecordDialogCreation(chrome::DialogIdentifier::DOWNLOAD_IN_PROGRESS);
}

DownloadInProgressDialogView::~DownloadInProgressDialogView() {}

gfx::Size DownloadInProgressDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

bool DownloadInProgressDialogView::Cancel() {
  callback_.Run(false /* cancel_downloads */);
  return true;
}

bool DownloadInProgressDialogView::Accept() {
  callback_.Run(true /* cancel_downloads */);
  return true;
}

ui::ModalType DownloadInProgressDialogView::GetModalType() const {
  return app_modal_ ? ui::MODAL_TYPE_SYSTEM : ui::MODAL_TYPE_WINDOW;
}

bool DownloadInProgressDialogView::ShouldShowCloseButton() const {
  return false;
}

base::string16 DownloadInProgressDialogView::GetWindowTitle() const {
  return l10n_util::GetPluralStringFUTF16(IDS_ABANDON_DOWNLOAD_DIALOG_TITLE,
                                          download_count_);
}
