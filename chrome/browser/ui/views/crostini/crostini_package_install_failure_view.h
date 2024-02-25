// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_PACKAGE_INSTALL_FAILURE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_PACKAGE_INSTALL_FAILURE_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace crostini {

// Shows the UI with the error message when installing a package fails.
void ShowCrostiniPackageInstallFailureView(const std::string& error_message);

}  // namespace crostini

// Displays error information when the user fails to install a package.
class CrostiniPackageInstallFailureView
    : public views::BubbleDialogDelegateView {
  METADATA_HEADER(CrostiniPackageInstallFailureView,
                  views::BubbleDialogDelegateView)

 public:
  static void Show(const std::string& error_message);

 private:
  explicit CrostiniPackageInstallFailureView(const std::string& error_message);

  ~CrostiniPackageInstallFailureView() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_PACKAGE_INSTALL_FAILURE_VIEW_H_
