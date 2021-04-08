// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/in_session_password_change/lock_screen_network_dialog.h"

#include <memory>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/strings/grit/ui_strings.h"

namespace chromeos {

LockScreenNetworkDialog::LockScreenNetworkDialog(
    NetworkDialogCleanupCallback callback)
    : BaseLockDialog(GURL(chrome::kChromeUILockScreenNetworkURL),
                     kBaseLockDialogSize) {
  callback_ = std::move(callback);
}

LockScreenNetworkDialog::~LockScreenNetworkDialog() = default;

void LockScreenNetworkDialog::OnDialogClosed(const std::string& json_retval) {
  base::PostTask(FROM_HERE, {content::BrowserThread::UI}, std::move(callback_));
}

void LockScreenNetworkDialog::Show(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ShowSystemDialogForBrowserContext(profile->GetPrimaryOTRProfile());
}

void LockScreenNetworkDialog::Dismiss() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Close();
}

}  // namespace chromeos
