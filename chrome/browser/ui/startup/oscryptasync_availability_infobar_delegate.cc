// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/oscryptasync_availability_infobar_delegate.h"

#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"

// static
void OSCryptAsyncAvailabilityInfoBarDelegate::MaybeCreate(
    BrowserWindowInterface* browser) {
  if (!base::FeatureList::IsEnabled(
          features::kOSCryptAsyncAvailabilityInfoBar)) {
    return;
  }
  g_browser_process->os_crypt_async()->GetInstance(base::BindOnce(
      [](base::WeakPtr<BrowserWindowInterface> browser,
         os_crypt_async::Encryptor encryptor) {
        if (encryptor.IsEncryptionAvailable() || !browser ||
            browser->GetTabStripModel()->empty()) {
          return;
        }
        content::WebContents* web_contents =
            browser->GetTabStripModel()->GetActiveWebContents();
        if (!web_contents) {
          return;
        }
        infobars::ContentInfoBarManager* infobar_manager =
            infobars::ContentInfoBarManager::FromWebContents(web_contents);
        if (!infobar_manager) {
          return;
        }
        infobar_manager->AddInfoBar(
            CreateConfirmInfoBar(std::unique_ptr<ConfirmInfoBarDelegate>(
                new OSCryptAsyncAvailabilityInfoBarDelegate())));
      },
      browser->GetWeakPtr()));
}

// static
void OSCryptAsyncAvailabilityInfoBarDelegate::CreateForTest(
    infobars::ContentInfoBarManager* infobar_manager) {
  infobar_manager->AddInfoBar(
      CreateConfirmInfoBar(std::unique_ptr<ConfirmInfoBarDelegate>(
          new OSCryptAsyncAvailabilityInfoBarDelegate())));
}

infobars::InfoBarDelegate::InfoBarIdentifier
OSCryptAsyncAvailabilityInfoBarDelegate::GetIdentifier() const {
  return OSCRYPTASYNC_AVAILABILITY_INFOBAR_DELEGATE;
}

infobars::InfoBarDelegate::InfobarPriority
OSCryptAsyncAvailabilityInfoBarDelegate::GetPriority() const {
  return infobars::InfoBarDelegate::InfobarPriority::kCriticalSecurity;
}

const gfx::VectorIcon& OSCryptAsyncAvailabilityInfoBarDelegate::GetVectorIcon()
    const {
  return vector_icons::kErrorIcon;
}

std::u16string OSCryptAsyncAvailabilityInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(
      IDS_OSCRYPTASYNC_AVAILABILITY_INFOBAR_MESSAGE);
}

int OSCryptAsyncAvailabilityInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

std::u16string OSCryptAsyncAvailabilityInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  CHECK_EQ(button, BUTTON_OK);
  return l10n_util::GetStringUTF16(
      IDS_OSCRYPTASYNC_AVAILABILITY_INFOBAR_BUTTON);
}

bool OSCryptAsyncAvailabilityInfoBarDelegate::Accept() {
  chrome::AttemptRelaunch();
  return false;
}

bool OSCryptAsyncAvailabilityInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

bool OSCryptAsyncAvailabilityInfoBarDelegate::IsCloseable() const {
  return false;
}
