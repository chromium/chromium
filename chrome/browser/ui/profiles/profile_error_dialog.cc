// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profiles/profile_error_dialog.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

#if !BUILDFLAG(IS_ANDROID) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr char kProfileErrorFeedbackCategory[] = "FEEDBACK_PROFILE_ERROR";

bool g_is_showing_profile_error_dialog = false;

void OnProfileErrorDialogDismissed(const std::string& diagnostics,
                                   bool needs_feedback) {
  g_is_showing_profile_error_dialog = false;

  if (!needs_feedback) {
    return;
  }

  std::string feedback_description =
      l10n_util::GetStringUTF8(IDS_PROFILE_ERROR_FEEDBACK_DESCRIPTION);

  chrome::ShowFeedbackPage(nullptr, feedback::kFeedbackSourceProfileErrorDialog,
                           feedback_description,
                           std::string() /* description_placeholder_text */,
                           kProfileErrorFeedbackCategory, diagnostics);
}
#endif  // !BUILDFLAG(IS_ANDROID) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace

void ShowProfileErrorDialog(ProfileErrorType type,
                            int message_id,
                            const std::string& diagnostics) {
#if BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED();
#else  // BUILDFLAG(IS_ANDROID)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kNoErrorDialogs)) {
    return;
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (g_is_showing_profile_error_dialog) {
    return;
  }

  g_is_showing_profile_error_dialog = true;
  chrome::ShowWarningMessageBoxWithCheckbox(
      nullptr, l10n_util::GetStringUTF16(IDS_PROFILE_ERROR_DIALOG_TITLE),
      l10n_util::GetStringUTF16(message_id),
      l10n_util::GetStringUTF16(IDS_PROFILE_ERROR_DIALOG_CHECKBOX),
      base::BindOnce(&OnProfileErrorDialogDismissed, diagnostics));
#else   // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  chrome::ShowWarningMessageBox(
      nullptr, l10n_util::GetStringUTF16(IDS_PROFILE_ERROR_DIALOG_TITLE),
      l10n_util::GetStringUTF16(message_id));
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

#endif  // BUILDFLAG(IS_ANDROID)
}
