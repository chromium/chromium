// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/keystone_infobar_delegate.h"

#import <AppKit/AppKit.h>

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#import "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#import "chrome/browser/mac/keystone_glue.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/cocoa/last_active_browser_cocoa.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/updater/browser_updater_client_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

class SkBitmap;

// KeystonePromotionInfoBarDelegate -------------------------------------------

// static
void KeystonePromotionInfoBarDelegate::Create(
    content::WebContents* webContents) {
  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(webContents);
  infobar_manager->AddInfoBar(
      CreateConfirmInfoBar(std::unique_ptr<ConfirmInfoBarDelegate>(
          new KeystonePromotionInfoBarDelegate(
              Profile::FromBrowserContext(webContents->GetBrowserContext())
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
  if (base::FeatureList::IsEnabled(features::kUseChromiumUpdater)) {
    SetupSystemUpdater();
  } else {
    [[KeystoneGlue defaultKeystoneGlue] promoteTicket];
  }
  return true;
}

bool KeystonePromotionInfoBarDelegate::Cancel() {
  prefs_->SetBoolean(prefs::kShowUpdatePromotionInfoBar, false);
  return true;
}

// KeystonePromotionInfoBar ---------------------------------------------------

@interface KeystonePromotionInfoBar : NSObject
- (void)checkAndShowInfoBarForProfile:(Profile*)profile;
- (void)updateStatus:(NSNotification*)notification;
- (void)removeObserver;
@end  // @interface KeystonePromotionInfoBar

KeystonePromotionInfoBar* g_currentPromotionInfoBar;

@implementation KeystonePromotionInfoBar

- (void)dealloc {
  [self removeObserver];
}

- (void)checkAndShowInfoBarForProfile:(Profile*)profile {
  // If this is the first run, the user clicked the "don't ask again" button
  // at some point in the past, or if the "don't ask about the default
  // browser" command-line switch is present, bail out.  That command-line
  // switch is recycled here because it's likely that the set of users that
  // don't want to be nagged about the default browser also don't want to be
  // nagged about the update check.  (Automated testers, I'm thinking of
  // you...)
  base::CommandLine* commandLine = base::CommandLine::ForCurrentProcess();
  if (first_run::IsChromeFirstRun() ||
      !profile->GetPrefs()->GetBoolean(prefs::kShowUpdatePromotionInfoBar) ||
      commandLine->HasSwitch(switches::kNoDefaultBrowserCheck)) {
    return;
  }

  // If there is no Keystone glue (maybe because this application isn't
  // Keystone-enabled) or the application is on a read-only filesystem,
  // doing anything related to auto-update is pointless.  Bail out.
  KeystoneGlue* keystoneGlue = [KeystoneGlue defaultKeystoneGlue];
  if (!keystoneGlue || [keystoneGlue isOnReadOnlyFilesystem]) {
    return;
  }

  if (base::FeatureList::IsEnabled(features::kUseChromiumUpdater)) {
    EnsureUpdater(base::BindOnce([]() {
                    Browser* browser = chrome::GetLastActiveBrowser();
                    if (browser) {
                      content::WebContents* webContents =
                          browser->tab_strip_model()->GetActiveWebContents();
                      if (webContents)
                        KeystonePromotionInfoBarDelegate::Create(webContents);
                    }
                  }),
                  base::DoNothing());
  } else {
    // Stay alive as long as needed.  This is balanced in -updateStatus:.
    g_currentPromotionInfoBar = self;

    AutoupdateStatus recentStatus = [keystoneGlue recentStatus];
    if (recentStatus == kAutoupdateNone ||
        recentStatus == kAutoupdateRegistering) {
      [NSNotificationCenter.defaultCenter
          addObserver:self
             selector:@selector(updateStatus:)
                 name:kAutoupdateStatusNotification
               object:nil];
    } else {
      [self updateStatus:[keystoneGlue recentNotification]];
    }
  }
}

- (void)updateStatus:(NSNotification*)notification {
  NSDictionary* dictionary = [notification userInfo];
  AutoupdateStatus status = static_cast<AutoupdateStatus>(
      [dictionary[kAutoupdateStatusStatus] intValue]);

  if (status == kAutoupdateNone || status == kAutoupdateRegistering) {
    return;
  }

  [self removeObserver];

  if (status != kAutoupdateRegisterFailed &&
      [[KeystoneGlue defaultKeystoneGlue] needsPromotion]) {
    Browser* browser = chrome::GetLastActiveBrowser();
    if (browser) {
      content::WebContents* webContents =
          browser->tab_strip_model()->GetActiveWebContents();
      if (webContents)
        KeystonePromotionInfoBarDelegate::Create(webContents);
    }
  }

  g_currentPromotionInfoBar = nil;
}

- (void)removeObserver {
  [NSNotificationCenter.defaultCenter removeObserver:self];
}

@end  // @implementation KeystonePromotionInfoBar

// static
void KeystoneInfoBar::PromotionInfoBar(Profile* profile) {
  KeystonePromotionInfoBar* promotionInfoBar =
      [[KeystonePromotionInfoBar alloc] init];

  [promotionInfoBar checkAndShowInfoBarForProfile:profile];
}
