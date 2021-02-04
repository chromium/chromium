// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_PACKAGE_INSTALL_FAILURE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_PACKAGE_INSTALL_FAILURE_VIEW_H_

#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/metadata/metadata_header_macros.h"

// Displays error information when the user fails to install a package.
class CrostiniPackageInstallFailureView
    : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(CrostiniPackageInstallFailureView);

  static void Show(const std::string& error_message);

 private:
  explicit CrostiniPackageInstallFailureView(const std::string& error_message);

  ~CrostiniPackageInstallFailureView() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_PACKAGE_INSTALL_FAILURE_VIEW_H_
