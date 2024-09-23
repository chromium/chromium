// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_DIAGNOSTICS_DIALOG_DIAGNOSTICS_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_DIAGNOSTICS_DIALOG_DIAGNOSTICS_DIALOG_H_

#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "ui/gfx/native_widget_types.h"

namespace ash {

namespace {

// ID used to lookup existing DiagnosticsDialog instance from
// SystemWebDialogDelegate list and ensure only one instance of
// DiagnosticsDialog exists at a time.
constexpr char kDiagnosticsDialogId[] = "diagnostics-dialog";

}  // namespace

class DiagnosticsDialog : public SystemWebDialogDelegate {
 public:
  // Denotes different sub-pages of the diagnostics app.
  enum class DiagnosticsPage {
    // The default page.
    kDefault,
    // The system page.
    kSystem,
    // The connectivity page.
    kConnectivity,
    // The input page.
    kInput
  };

  // |page| is the initial page shown when the app is opened.
  static void ShowDialog(DiagnosticsPage page = DiagnosticsPage::kDefault,
                         gfx::NativeWindow parent = gfx::NativeWindow());

  // Closes an existing Diagnostics dialog if it exists.
  static void MaybeCloseExistingDialog();

 protected:
  explicit DiagnosticsDialog(DiagnosticsPage page);
  ~DiagnosticsDialog() override;

  DiagnosticsDialog(const DiagnosticsDialog&) = delete;
  DiagnosticsDialog& operator=(const DiagnosticsDialog&) = delete;

  // SystemWebDialogDelegate
  std::string Id() override;
  bool ShouldCloseDialogOnEscape() const override;

  // ui::WebDialogDelegate
  void GetDialogSize(gfx::Size* size) const override;

 private:
  const std::string dialog_id_ = kDiagnosticsDialogId;
  friend class DiagnosticsDialogTest;
};
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_DIAGNOSTICS_DIALOG_DIAGNOSTICS_DIALOG_H_
