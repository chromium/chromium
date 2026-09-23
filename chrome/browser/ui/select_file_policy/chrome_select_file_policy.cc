// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/select_file_policy/chrome_select_file_policy.h"

#include "base/check.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "ui/android/window_android.h"
#else
#include "chrome/browser/infobars/simple_alert_infobar_creator.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar_delegate.h"
#endif

ChromeSelectFilePolicy::ChromeSelectFilePolicy(
    content::WebContents* source_contents)
    : source_contents_(source_contents) {}

ChromeSelectFilePolicy::~ChromeSelectFilePolicy() = default;

bool ChromeSelectFilePolicy::CanOpenSelectFileDialog() {
  if (source_contents_) {
    Profile* profile =
        Profile::FromBrowserContext(source_contents_->GetBrowserContext());
    if (profile && profile->GetPrefs()) {
      const PrefService::Preference* pref = profile->GetPrefs()->FindPreference(
          prefs::kAllowFileSelectionDialogs);
      if (pref &&
          !profile->GetPrefs()->GetBoolean(prefs::kAllowFileSelectionDialogs)) {
        return false;
      }
    }
  }
  return FileSelectDialogsAllowed();
}

void ChromeSelectFilePolicy::SelectFileDenied() {
  // If the WebContents is in a browser window, show a toast / infobar saying
  // that file selection dialogs are disabled.
  if (source_contents_) {
#if BUILDFLAG(IS_ANDROID)
    // Show a toast on Clank instead of an infobar.
    if (auto* window = source_contents_->GetTopLevelNativeWindow()) {
      window->ShowToast(
          l10n_util::GetStringUTF8(IDS_FILE_SELECTION_DIALOG_INFOBAR));
    }
#else
    infobars::ContentInfoBarManager* infobar_manager =
        infobars::ContentInfoBarManager::FromWebContents(source_contents_);
    if (infobar_manager) {
      CreateSimpleAlertInfoBar(
          infobar_manager,
          infobars::InfoBarDelegate::FILE_ACCESS_DISABLED_INFOBAR_DELEGATE,
          nullptr,
          l10n_util::GetStringUTF16(IDS_FILE_SELECTION_DIALOG_INFOBAR));
    }
#endif
  }
}

// static
bool ChromeSelectFilePolicy::FileSelectDialogsAllowed() {
  DCHECK(g_browser_process);

  // local_state() can return NULL for tests.
  if (!g_browser_process->local_state()) {
    return false;
  }

  return !g_browser_process->local_state()->FindPreference(
             prefs::kAllowFileSelectionDialogs) ||
         g_browser_process->local_state()->GetBoolean(
             prefs::kAllowFileSelectionDialogs);
}
