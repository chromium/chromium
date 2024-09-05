// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SANITIZE_DIALOG_SANITIZE_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SANITIZE_DIALOG_SANITIZE_DIALOG_H_

#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "ui/gfx/native_widget_types.h"

namespace ash {

namespace {

// ID used to check if there are any other instances of the dialog open.
constexpr char kSanitizeDialogId[] = "sanitize-dialog";

}  // namespace

class SanitizeDialog : public SystemWebDialogDelegate {
 public:
  // Used to differentiate between different pages in the app. Initially the app
  // opens up on the initial page which communicates to the user what the
  // feature does. Once the feature is run, the app will open once again, this
  // time to the done page to show what has changed.
  enum class SanitizePage { kInitial, kDone };
  // `page` is the initial page shown when the app is opened.
  static void ShowDialog(SanitizePage page = SanitizePage::kInitial,
                         gfx::NativeWindow parent = gfx::NativeWindow());

  // Closes an existing dialog if it exists.
  static void MaybeCloseExistingDialog();

 protected:
  explicit SanitizeDialog(SanitizePage page);
  ~SanitizeDialog() override;

  SanitizeDialog(const SanitizeDialog&) = delete;
  SanitizeDialog& operator=(const SanitizeDialog&) = delete;

  // SystemWebDialogDelegate
  std::string Id() override;

  // ui::WebDialogDelegate
  void GetDialogSize(gfx::Size* size) const override;

 private:
  const std::string dialog_id_ = kSanitizeDialogId;
};
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SANITIZE_DIALOG_SANITIZE_DIALOG_H_
