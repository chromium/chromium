// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/keystone_infobar_delegate.h"

#include <string>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/updater/browser_updater_client_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

void ShowUpdaterPromotionInfoBarOnUISequence() {
  // If the user clicked the "don't ask again" button at some point in the
  // past, or if the "don't ask about the default browser" command-line switch
  // is present, bail out.  That command-line switch is recycled here because
  // it's likely that the set of users that don't want to be nagged about the
  // default browser also don't want to be nagged about the update check.
  // (Automated testers, I'm thinking of you...)
  Browser* browser = chrome::FindLastActive();
  if (!browser || !browser->profile() ||
      !browser->profile()->GetPrefs()->GetBoolean(
          prefs::kShowUpdatePromotionInfoBar) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kNoDefaultBrowserCheck)) {
    return;
  }
  KeystonePromotionInfoBarDelegate::Create(
      browser->tab_strip_model()->GetActiveWebContents());
}

}  // namespace

// KeystonePromotionInfoBarDelegate -------------------------------------------

// static
void KeystonePromotionInfoBarDelegate::Create(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }
  infobars::ContentInfoBarManager::FromWebContents(web_contents)
      ->AddInfoBar(CreateConfirmInfoBar(std::unique_ptr<ConfirmInfoBarDelegate>(
          new KeystonePromotionInfoBarDelegate(
              Profile::FromBrowserContext(web_contents->GetBrowserContext())
                  ->GetPrefs()))));
}

KeystonePromotionInfoBarDelegate::KeystonePromotionInfoBarDelegate(
    PrefService* prefs)
    : prefs_(prefs), can_expire_(false), weak_ptr_factory_(this) {
  const base::TimeDelta kCanExpireOnNavigationAfterDelay = base::Seconds(8);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&KeystonePromotionInfoBarDelegate::SetCanExpire,
                     weak_ptr_factory_.GetWeakPtr()),
      kCanExpireOnNavigationAfterDelay);
}

KeystonePromotionInfoBarDelegate::~KeystonePromotionInfoBarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
KeystonePromotionInfoBarDelegate::GetIdentifier() const {
  return KEYSTONE_PROMOTION_INFOBAR_DELEGATE_MAC;
}

int KeystonePromotionInfoBarDelegate::GetIconId() const {
  return IDR_PRODUCT_LOGO_32;
}

bool KeystonePromotionInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return can_expire_ && ConfirmInfoBarDelegate::ShouldExpire(details);
}

std::u16string KeystonePromotionInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_PROMOTE_INFOBAR_TEXT,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
}

std::u16string KeystonePromotionInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_PROMOTE_INFOBAR_PROMOTE_BUTTON : IDS_PROMOTE_INFOBAR_DONT_ASK_BUTTON);
}

bool KeystonePromotionInfoBarDelegate::Accept() {
  SetupSystemUpdater();
  return true;
}

bool KeystonePromotionInfoBarDelegate::Cancel() {
  prefs_->SetBoolean(prefs::kShowUpdatePromotionInfoBar, false);
  return true;
}

void ShowUpdaterPromotionInfoBar() {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ShowUpdaterPromotionInfoBarOnUISequence));
}
