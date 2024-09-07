// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_OS_FEEDBACK_DIALOG_OS_FEEDBACK_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_OS_FEEDBACK_DIALOG_OS_FEEDBACK_DIALOG_H_

#include "base/functional/callback_forward.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "extensions/common/api/feedback_private.h"

namespace ash {

class OsFeedbackDialog : public SystemWebDialogDelegate {
 public:
  // Shows a Feedback dialog using the specified BrowserContext (or Profile).
  // If |parent| is not null, the dialog will be parented to |parent|.
  // Otherwise it will be attached to either the AlwaysOnTop container or the
  // LockSystemModal container, depending on the session state at creation.
  // The `callback` will be invoked after a new dialog or an existing one has
  // been shown.
  static void ShowDialogAsync(
      content::BrowserContext* context,
      const extensions::api::feedback_private::FeedbackInfo& info,
      base::OnceClosure callback,
      gfx::NativeWindow parent = nullptr);

  // Find the native window of the dialog.
  static gfx::NativeWindow FindDialogWindow();

 protected:
  explicit OsFeedbackDialog(base::Value::Dict feedback_info);
  OsFeedbackDialog(const OsFeedbackDialog&) = delete;
  OsFeedbackDialog& operator=(const OsFeedbackDialog&) = delete;
  ~OsFeedbackDialog() override;

 private:
  // SystemWebDialogDelegate:
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;

  // Used to populate feedback context when launched from Dialog (eg. from login
  // screen).
  const base::Value::Dict feedback_info_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_OS_FEEDBACK_DIALOG_OS_FEEDBACK_DIALOG_H_
