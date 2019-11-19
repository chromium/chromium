// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_IN_PROGRESS_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_IN_PROGRESS_DIALOG_VIEW_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/browser.h"
#include "ui/views/window/dialog_delegate.h"

// Dialog shown when the user tries to exit the browser or all incognito windows
// while a download is in progress.
class DownloadInProgressDialogView : public views::DialogDelegateView {
 public:
  // |dialog_type| should be either DOWNLOAD_CLOSE_BROWSER_SHUTDOWN to indicate
  // the user is closing the browser or
  // DOWNLOAD_CLOSE_LAST_WINDOW_IN_INCOGNITO_PROFILE to indicate the user is
  // closing the last incognito window. |callback| will be called with true if
  // the download should be canceled, or false if the download should proceed.
  static void Show(gfx::NativeWindow parent_window,
                   int download_count,
                   Browser::DownloadCloseType dialog_type,
                   bool app_modal,
                   const base::Callback<void(bool)>& callback);

 private:
  DownloadInProgressDialogView(int download_count,
                               Browser::DownloadCloseType dialog_type,
                               bool app_modal,
                               const base::Callback<void(bool)>& callback);
  ~DownloadInProgressDialogView() override;

  // views::DialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  bool Cancel() override;
  bool Accept() override;
  ui::ModalType GetModalType() const override;
  bool ShouldShowCloseButton() const override;
  base::string16 GetWindowTitle() const override;

  const int download_count_;
  const bool app_modal_;
  const base::Callback<void(bool)> callback_;

  DISALLOW_COPY_AND_ASSIGN(DownloadInProgressDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_IN_PROGRESS_DIALOG_VIEW_H_
