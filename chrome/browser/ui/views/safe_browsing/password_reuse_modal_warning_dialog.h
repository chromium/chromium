// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_PASSWORD_REUSE_MODAL_WARNING_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_PASSWORD_REUSE_MODAL_WARNING_DIALOG_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
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
  METADATA_HEADER(PasswordReuseModalWarningDialog, views::DialogDelegateView)

 public:
  PasswordReuseModalWarningDialog(content::WebContents* web_contents,
                                  ChromePasswordProtectionService* service,
                                  ReusedPasswordAccountType password_type,
                                  OnWarningDone done_callback);
  PasswordReuseModalWarningDialog(const PasswordReuseModalWarningDialog&) =
      delete;
  PasswordReuseModalWarningDialog& operator=(
      const PasswordReuseModalWarningDialog&) = delete;
  ~PasswordReuseModalWarningDialog() override;

  void CreateSavedPasswordReuseModalWarningDialog(
      const std::u16string message_body);
  void CreateGaiaPasswordReuseModalWarningDialog(
      views::Label* message_body_label);

  // views::DialogDelegateView:
  gfx::Size GetMinimumSize() const override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  std::u16string GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  ui::ImageModel GetWindowIcon() override;

  // ChromePasswordProtectionService::Observer:
  void OnGaiaPasswordChanged() override;
  void OnMarkingSiteAsLegitimate(const GURL& url) override;
  void InvokeActionForTesting(WarningAction action) override;
  WarningUIType GetObserverType() override;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

 private:
  OnWarningDone done_callback_;
  raw_ptr<ChromePasswordProtectionService> service_;
  const GURL url_;
  const ReusedPasswordAccountType password_type_;

  // Records the start time when modal warning is constructed.
  base::TimeTicks modal_construction_start_time_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_UI_VIEWS_SAFE_BROWSING_PASSWORD_REUSE_MODAL_WARNING_DIALOG_H_
