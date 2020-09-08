// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_PASSWORD_REUSE_MODAL_WARNING_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_PASSWORD_REUSE_MODAL_WARNING_DIALOG_H_

#include "base/callback.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/controls/label.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}  // namespace content

namespace safe_browsing {

using password_manager::metrics_util::PasswordType;

// Implementation of password reuse modal dialog.
class PasswordReuseModalWarningDialog
    : public views::DialogDelegateView,
      public ChromePasswordProtectionService::Observer,
      public content::WebContentsObserver {
 public:
  PasswordReuseModalWarningDialog(content::WebContents* web_contents,
                                  ChromePasswordProtectionService* service,
                                  ReusedPasswordAccountType password_type,
                                  OnWarningDone done_callback);

  ~PasswordReuseModalWarningDialog() override;

  void CreateSavedPasswordReuseModalWarningDialog(
      const base::string16 message_body,
      std::vector<base::string16> placeholders,
      std::vector<size_t> placeholder_offsets);
  void CreateGaiaPasswordReuseModalWarningDialog(
      views::Label* message_body_label);

  // views::DialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  gfx::ImageSkia GetWindowIcon() override;

  // ChromePasswordProtectionService::Observer:
  void OnGaiaPasswordChanged() override;
  void OnMarkingSiteAsLegitimate(const GURL& url) override;
  void InvokeActionForTesting(WarningAction action) override;
  WarningUIType GetObserverType() override;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

 private:
  OnWarningDone done_callback_;
  ChromePasswordProtectionService* service_;
  const GURL url_;
  const ReusedPasswordAccountType password_type_;

  // Records the start time when modal warning is constructed.
  base::TimeTicks modal_construction_start_time_;

  DISALLOW_COPY_AND_ASSIGN(PasswordReuseModalWarningDialog);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_PASSWORD_REUSE_MODAL_WARNING_DIALOG_H_
