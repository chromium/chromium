// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chrome_select_file_policy.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/simple_alert_infobar_creator.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

ChromeSelectFilePolicy::ChromeSelectFilePolicy(
    content::WebContents* source_contents)
    : source_contents_(source_contents) {}

ChromeSelectFilePolicy::~ChromeSelectFilePolicy() {}

bool ChromeSelectFilePolicy::CanOpenSelectFileDialog() {
  return FileSelectDialogsAllowed();
}

void ChromeSelectFilePolicy::SelectFileDenied() {
  // If the WebContents is in a browser window, show an infobar saying that
  // file selection dialogs are disabled.
  if (source_contents_) {
    infobars::ContentInfoBarManager* infobar_manager =
        infobars::ContentInfoBarManager::FromWebContents(source_contents_);
    if (infobar_manager) {
      CreateSimpleAlertInfoBar(
          infobar_manager,
          infobars::InfoBarDelegate::FILE_ACCESS_DISABLED_INFOBAR_DELEGATE,
          nullptr,
          l10n_util::GetStringUTF16(IDS_FILE_SELECTION_DIALOG_INFOBAR));
    }
  }
}

// static
bool ChromeSelectFilePolicy::FileSelectDialogsAllowed() {
  DCHECK(g_browser_process);

  // local_state() can return NULL for tests.
  if (!g_browser_process->local_state())
    return false;

  return !g_browser_process->local_state()->FindPreference(
             prefs::kAllowFileSelectionDialogs) ||
         g_browser_process->local_state()->GetBoolean(
             prefs::kAllowFileSelectionDialogs);
}
