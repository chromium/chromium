// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_infobar_delegate.h"

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/common/content_switches.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/win/taskbar_manager.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#endif

namespace {

#if BUILDFLAG(IS_WIN)
void PinToTaskbarResult(bool pinned) {
  // TODO(crbug.com/343734031): Emit a metric with the pin result. Initially,
  // taskbar_manager.cc metrics will suffice, but taskbar_manager will most
  // likely get used by other code.
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

// static
infobars::InfoBar* DefaultBrowserInfoBarDelegate::Create(
    infobars::ContentInfoBarManager* infobar_manager,
    Profile* profile,
    bool can_pin_to_taskbar) {
  return infobar_manager->AddInfoBar(
      CreateConfirmInfoBar(std::make_unique<DefaultBrowserInfoBarDelegate>(
          base::PassKey<DefaultBrowserInfoBarDelegate>(), profile,
          can_pin_to_taskbar)));
}

DefaultBrowserInfoBarDelegate::DefaultBrowserInfoBarDelegate(
    base::PassKey<DefaultBrowserInfoBarDelegate>,
    Profile* profile,
    bool can_pin_to_taskbar)
    : profile_(profile), can_pin_to_taskbar_(can_pin_to_taskbar) {}

DefaultBrowserInfoBarDelegate::~DefaultBrowserInfoBarDelegate() {
  if (!action_taken_) {
    base::RecordAction(base::UserMetricsAction("DefaultBrowserInfoBar_Ignore"));
    UMA_HISTOGRAM_ENUMERATION("DefaultBrowser.InfoBar.UserInteraction",
                              IGNORE_INFO_BAR,
                              NUM_INFO_BAR_USER_INTERACTION_TYPES);
  }
}

infobars::InfoBarDelegate::InfoBarIdentifier
DefaultBrowserInfoBarDelegate::GetIdentifier() const {
  return DEFAULT_BROWSER_INFOBAR_DELEGATE;
}

const gfx::VectorIcon& DefaultBrowserInfoBarDelegate::GetVectorIcon() const {
  return dark_mode() ? omnibox::kProductChromeRefreshIcon
                     : vector_icons::kProductRefreshIcon;
}

bool DefaultBrowserInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

void DefaultBrowserInfoBarDelegate::InfoBarDismissed() {
  action_taken_ = true;
  // |profile_| may be null in tests.
  if (profile_) {
    chrome::startup::default_prompt::UpdatePrefsForDismissedPrompt(profile_);
  }
  base::RecordAction(base::UserMetricsAction("DefaultBrowserInfoBar_Dismiss"));
  UMA_HISTOGRAM_ENUMERATION("DefaultBrowser.InfoBar.UserInteraction",
                            DISMISS_INFO_BAR,
                            NUM_INFO_BAR_USER_INTERACTION_TYPES);

  ConfirmInfoBarDelegate::InfoBarDismissed();
}

std::u16string DefaultBrowserInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(can_pin_to_taskbar_
                                       ? IDS_DEFAULT_BROWSER_PIN_INFOBAR_TEXT
                                       : IDS_DEFAULT_BROWSER_INFOBAR_TEXT);
}

int DefaultBrowserInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

std::u16string DefaultBrowserInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_INFOBAR_OK_BUTTON_LABEL);
}

bool DefaultBrowserInfoBarDelegate::Accept() {
  action_taken_ = true;
  // |profile_| may be null in tests.
  if (profile_) {
    chrome::startup::default_prompt::UpdatePrefsForDismissedPrompt(profile_);
  }
  base::RecordAction(base::UserMetricsAction("DefaultBrowserInfoBar_Accept"));
  UMA_HISTOGRAM_ENUMERATION("DefaultBrowser.InfoBar.UserInteraction",
                            ACCEPT_INFO_BAR,
                            NUM_INFO_BAR_USER_INTERACTION_TYPES);

  // The worker pointer is reference counted. While it is running, the
  // message loops of the FILE and UI thread will hold references to
  // it and it will be automatically freed once all its tasks have
  // finished.
  base::MakeRefCounted<shell_integration::DefaultBrowserWorker>()
      ->StartSetAsDefault(base::DoNothing());
  if (can_pin_to_taskbar_) {
#if BUILDFLAG(IS_WIN)
    // Attempt the pin to taskbar in parallel with bringing up the Windows
    // settings UI. Serializing the operations is an option, but since the user
    // might not complete the first operation, serializing would probably make
    // the second operation less likely to happen.
    browser_util::PinAppToTaskbar(
        ShellUtil::GetBrowserModelId(InstallUtil::IsPerUserInstall()),
        base::BindOnce(&PinToTaskbarResult));
#else
    NOTREACHED();
#endif  // BUILDFLAG(IS_WIN)
  }

  return ConfirmInfoBarDelegate::Accept();
}

bool DefaultBrowserInfoBarDelegate::ShouldHideInFullscreen() const {
  return true;
}
