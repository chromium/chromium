// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/rosetta_required_infobar_delegate.h"

#import <AppKit/AppKit.h>

#include <memory>

#include "base/bind.h"
#include "base/mac/rosetta.h"
#include "base/mac/scoped_nsobject.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/component_updater/widevine_cdm_component_installer.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/widevine/cdm/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

// static
bool RosettaRequiredInfoBarDelegate::ShouldShow() {
#if defined(ARCH_CPU_ARM64) && BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)
  return component_updater::WasWidevineCdmComponentRejectedDueToNoRosetta();
#else
  return false;
#endif  // ARCH_CPU_ARM64
}

// static
void RosettaRequiredInfoBarDelegate::Create(
    content::WebContents* web_contents) {
  auto* infobar_service = InfoBarService::FromWebContents(web_contents);

  if (infobar_service) {
    infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
        std::make_unique<RosettaRequiredInfoBarDelegate>()));
  }
}

RosettaRequiredInfoBarDelegate::RosettaRequiredInfoBarDelegate() = default;
RosettaRequiredInfoBarDelegate::~RosettaRequiredInfoBarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
RosettaRequiredInfoBarDelegate::GetIdentifier() const {
  return ROSETTA_REQUIRED_INFOBAR_DELEGATE;
}

base::string16 RosettaRequiredInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_ROSETTA_REQUIRED_INFOBAR_LINK_TEXT);
}

GURL RosettaRequiredInfoBarDelegate::GetLinkURL() const {
  return GURL("https://support.google.com/chrome/?p=mac_ARM");
}

base::string16 RosettaRequiredInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_ROSETTA_REQUIRED_INFOBAR_TEXT);
}

int RosettaRequiredInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

base::string16 RosettaRequiredInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(IDS_ROSETTA_REQUIRED_INFOBAR_BUTTON_TEXT);
}

bool RosettaRequiredInfoBarDelegate::Accept() {
#if defined(ARCH_CPU_ARM64)
  base::mac::RequestRosettaInstallation(
      l10n_util::GetStringUTF16(IDS_ROSETTA_INSTALL_TITLE),
      l10n_util::GetStringUTF16(IDS_ROSETTA_INSTALL_BODY),
      base::BindOnce([](base::mac::RosettaInstallationResult result) {
        if (result !=
                base::mac::RosettaInstallationResult::kInstallationSuccess &&
            result != base::mac::RosettaInstallationResult::kAlreadyInstalled) {
          return;
        }

        base::scoped_nsobject<NSAlert> alert([[NSAlert alloc] init]);
        [alert setMessageText:l10n_util::GetNSString(
                                  IDS_ROSETTA_INSTALL_RESTART_DIALOG)];
        [alert addButtonWithTitle:l10n_util::GetNSString(
                                      IDS_ROSETTA_INSTALL_RESTART_DIALOG_NOW)];
        [alert
            addButtonWithTitle:l10n_util::GetNSString(
                                   IDS_ROSETTA_INSTALL_RESTART_DIALOG_LATER)];
        NSModalResponse response = [alert runModal];

        if (response == NSAlertFirstButtonReturn) {
          base::ThreadTaskRunnerHandle::Get()->PostTask(
              FROM_HERE, base::BindOnce(&chrome::AttemptRestart));
        }
      }));
#endif  // ARCH_CPU_ARM64
  return true;
}
