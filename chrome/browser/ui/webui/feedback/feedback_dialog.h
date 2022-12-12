// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEEDBACK_FEEDBACK_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_FEEDBACK_FEEDBACK_DIALOG_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "extensions/common/api/feedback_private.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/widget/widget.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

class Profile;

class FeedbackDialog : public ui::WebDialogDelegate {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kFeedbackDialogForTesting);

  static void CreateOrShow(
      Profile* profile,
      const extensions::api::feedback_private::FeedbackInfo& info);

  FeedbackDialog(const FeedbackDialog&) = delete;
  FeedbackDialog& operator=(const FeedbackDialog&) = delete;
  ~FeedbackDialog() override;

  // Show this web dialog
  void Show() const;
  views::Widget* GetWidget() const;

  // Use for test. It could return a nullptr.
  static FeedbackDialog* GetInstanceForTest();

 private:
  explicit FeedbackDialog(
      Profile* profile,
      const extensions::api::feedback_private::FeedbackInfo& info);

  // Overrides from ui::WebDialogDelegate
  ui::ModalType GetDialogModalType() const override;
  std::u16string GetDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool ShouldShowDialogTitle() const override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const GURL& security_origin,
                                  blink::mojom::MediaStreamType type) override;

  base::Value::Dict feedback_info_;
  extensions::api::feedback_private::FeedbackFlow feedback_flow_;
  // Widget for the Feedback WebUI.
  raw_ptr<views::Widget> widget_;
  static FeedbackDialog* current_instance_;
  // Whether `this` should correspond to `current_instance_`.
  bool attached_to_current_instance_ = true;

  // Prevent Profile destruction until the dialog is closed, to prevent a
  // dangling RenderProcessHost crash.
  ScopedProfileKeepAlive profile_keep_alive_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEEDBACK_FEEDBACK_DIALOG_H_
