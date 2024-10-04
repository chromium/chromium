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
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_prefs.h"
#include "chrome/browser/ui/ui_features.h"
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

// static
infobars::InfoBar* DefaultBrowserInfoBarDelegate::Create(
    infobars::ContentInfoBarManager* infobar_manager,
    Profile* profile) {
  return infobar_manager->AddInfoBar(
      CreateConfirmInfoBar(std::make_unique<DefaultBrowserInfoBarDelegate>(
          base::PassKey<DefaultBrowserInfoBarDelegate>(), profile)));
}

DefaultBrowserInfoBarDelegate::DefaultBrowserInfoBarDelegate(
    base::PassKey<DefaultBrowserInfoBarDelegate>,
    Profile* profile)
    : profile_(profile) {
  if (!base::FeatureList::IsEnabled(features::kDefaultBrowserPromptRefresh)) {
    // We want the info-bar to stick-around for few seconds and then be hidden
    // on the next navigation after that.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DefaultBrowserInfoBarDelegate::AllowExpiry,
                       weak_factory_.GetWeakPtr()),
        base::Seconds(8));
  }
}

DefaultBrowserInfoBarDelegate::~DefaultBrowserInfoBarDelegate() {
  if (!action_taken_) {
    base::RecordAction(base::UserMetricsAction("DefaultBrowserInfoBar_Ignore"));
    UMA_HISTOGRAM_ENUMERATION("DefaultBrowser.InfoBar.UserInteraction",
                              IGNORE_INFO_BAR,
                              NUM_INFO_BAR_USER_INTERACTION_TYPES);
  }
}

void DefaultBrowserInfoBarDelegate::AllowExpiry() {
  should_expire_ = true;
}

infobars::InfoBarDelegate::InfoBarIdentifier
DefaultBrowserInfoBarDelegate::GetIdentifier() const {
  return DEFAULT_BROWSER_INFOBAR_DELEGATE;
}

const gfx::VectorIcon& DefaultBrowserInfoBarDelegate::GetVectorIcon() const {
  return dark_mode() ? omnibox::kProductChromeRefreshIcon
                     : vector_icons::kProductIcon;
}

bool DefaultBrowserInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return should_expire_ && ConfirmInfoBarDelegate::ShouldExpire(details);
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
  if (base::FeatureList::IsEnabled(features::kDefaultBrowserPromptRefresh) &&
      features::kUpdatedInfoBarCopy.Get()) {
    return l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_INFOBAR_REFRESH_TEXT);
  }
  return l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_INFOBAR_TEXT);
}

int DefaultBrowserInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

std::u16string DefaultBrowserInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  if (base::FeatureList::IsEnabled(features::kDefaultBrowserPromptRefresh) &&
      features::kUpdatedInfoBarCopy.Get()) {
    return l10n_util::GetStringUTF16(
        IDS_DEFAULT_BROWSER_INFOBAR_REFRESH_OK_BUTTON_LABEL);
  }
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

  return ConfirmInfoBarDelegate::Accept();
}
