// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EULA_DIALOG_LINUX_H_
#define CHROME_BROWSER_UI_VIEWS_EULA_DIALOG_LINUX_H_

#include "base/functional/callback.h"
#include "ui/views/window/dialog_delegate.h"

// A dialog that displays the End User License Agreement.
class EulaDialog : public views::DialogDelegate {
 public:
  explicit EulaDialog(base::OnceCallback<void(bool)> callback);
  EulaDialog(const EulaDialog&) = delete;
  EulaDialog& operator=(const EulaDialog&) = delete;
  ~EulaDialog() override;

  static views::Widget* Show(base::OnceCallback<void(bool)> callback);

  // views::DialogDelegate:
  bool Accept() override;
  bool Cancel() override;
  void WindowClosing() override;

 private:
  base::OnceCallback<void(bool)> callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EULA_DIALOG_LINUX_H_
