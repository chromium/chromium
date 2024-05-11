// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_PRINT_PREVIEW_CROS_PRINT_PREVIEW_CROS_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_PRINT_PREVIEW_CROS_PRINT_PREVIEW_CROS_DIALOG_H_

#include <string>

#include "base/unguessable_token.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog_delegate.h"

namespace ash::printing::print_preview {

// System delegate
class PrintPreviewCrosDialog : public SystemWebDialogDelegate {
 public:
  ~PrintPreviewCrosDialog() override = default;

  static PrintPreviewCrosDialog* ShowDialog(base::UnguessableToken token);

  // SystemWebDialogDelegate:
  void OnDialogShown(content::WebUI* webui) override;

 protected:
  explicit PrintPreviewCrosDialog(base::UnguessableToken token);

 private:
  // SystemWebDialogDelegate:
  std::string Id() override;

  base::UnguessableToken dialog_id_;
};

}  // namespace ash::printing::print_preview

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_PRINT_PREVIEW_CROS_PRINT_PREVIEW_CROS_DIALOG_H_
