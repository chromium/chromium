// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_network_dialog.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/task/task_traits.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_reauth_dialogs.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/strings/grit/ui_strings.h"

namespace ash {

LockScreenNetworkDialog::LockScreenNetworkDialog(
    NetworkDialogCleanupCallback callback)
    : BaseLockDialog(
          GURL(chrome::kChromeUILockScreenNetworkURL),
          LockScreenStartReauthDialog::CalculateLockScreenReauthDialogSize(
              features::IsNewLockScreenReauthLayoutEnabled())) {
  callback_ = std::move(callback);
}

LockScreenNetworkDialog::~LockScreenNetworkDialog() = default;

void LockScreenNetworkDialog::OnDialogClosed(const std::string& json_retval) {
  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(callback_));
}

void LockScreenNetworkDialog::Show(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(ProfileHelper::IsLockScreenProfile(profile));
  ShowSystemDialogForBrowserContext(profile);
}

void LockScreenNetworkDialog::Dismiss() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Close();
}

}  // namespace ash
