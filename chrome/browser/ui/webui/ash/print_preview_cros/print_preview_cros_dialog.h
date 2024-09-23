// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_PRINT_PREVIEW_CROS_PRINT_PREVIEW_CROS_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_PRINT_PREVIEW_CROS_PRINT_PREVIEW_CROS_DIALOG_H_

#include <string>

#include "base/observer_list_types.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "ui/gfx/native_widget_types.h"

namespace ash::printing::print_preview {

// Print preview dialog implementation. A SystemWebDialog to inherit base
// behaviors.
class PrintPreviewCrosDialog : public SystemWebDialogDelegate {
 public:
  // Notifies clients of events related to lifetime of dialog.
  class PrintPreviewCrosDialogObserver : public base::CheckedObserver {
   public:
    ~PrintPreviewCrosDialogObserver() override = default;
    virtual void OnDialogClosed(base::UnguessableToken token) = 0;
  };

  ~PrintPreviewCrosDialog() override;

  static PrintPreviewCrosDialog* ShowDialog(base::UnguessableToken token);

  void AddObserver(PrintPreviewCrosDialogObserver* observer);
  void RemoveObserver(PrintPreviewCrosDialogObserver* observer);

  // SystemWebDialogDelegate:
  void OnDialogShown(content::WebUI* webui) override;
  void OnDialogClosed(const std::string& json_retval) override;

  gfx::NativeWindow GetDialogWindowForTesting();

 protected:
  friend class PrintPreviewCrosDialogTest;

  explicit PrintPreviewCrosDialog(base::UnguessableToken token);

 private:
  // SystemWebDialogDelegate:
  std::string Id() override;

  base::ObserverList<PrintPreviewCrosDialogObserver> observer_list_;
  base::UnguessableToken dialog_id_;
};

}  // namespace ash::printing::print_preview

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_PRINT_PREVIEW_CROS_PRINT_PREVIEW_CROS_DIALOG_H_
