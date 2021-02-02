// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/mac_system_infobar_delegate.h"
#include "base/command_line.h"
#include "base/mac/mac_util.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using CPUType = base::mac::CPUType;

CPUType MaybeOverrideCPUTypeFromCommandLine(CPUType detected_type) {
  // Use like: --force-mac-system-infobar={arm,rosetta,intel} to see different
  // behavior by platform.
  std::string forced_cpu =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          "force-mac-system-infobar");
  if (forced_cpu == "arm")
    return CPUType::kArm;
  if (forced_cpu == "rosetta")
    return CPUType::kTranslatedIntel;
  if (forced_cpu == "intel")
    return CPUType::kIntel;
  return detected_type;
}

CPUType GetEffectiveCPUType() {
  return MaybeOverrideCPUTypeFromCommandLine(base::mac::GetCPUType());
}

}  // namespace

constexpr base::Feature MacSystemInfoBarDelegate::kMacSystemInfoBar;

// static
bool MacSystemInfoBarDelegate::ShouldShow() {
  constexpr base::FeatureParam<bool> kEnableRosetta{&kMacSystemInfoBar,
                                                    "enable_rosetta", false};
  constexpr base::FeatureParam<bool> kEnableArm{&kMacSystemInfoBar,
                                                "enable_arm", false};

  CPUType cpu = GetEffectiveCPUType();
  return (cpu == CPUType::kTranslatedIntel && kEnableRosetta.Get()) ||
         (cpu == CPUType::kArm && kEnableArm.Get());
}

// static
void MacSystemInfoBarDelegate::Create(InfoBarService* infobar_service) {
  infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate>(new MacSystemInfoBarDelegate())));
}

MacSystemInfoBarDelegate::MacSystemInfoBarDelegate() = default;
MacSystemInfoBarDelegate::~MacSystemInfoBarDelegate() = default;

bool MacSystemInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  // Don't dismiss on navigation - require an explicit dismissal. This infobar
  // communicates an important thing (i.e. that the product is probably kind of
  // broken).
  return false;
}

infobars::InfoBarDelegate::InfoBarIdentifier
MacSystemInfoBarDelegate::GetIdentifier() const {
  return SYSTEM_INFOBAR_DELEGATE_MAC;
}

base::string16 MacSystemInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(
      GetEffectiveCPUType() == CPUType::kArm
          ? IDS_MAC_SYSTEM_INFOBAR_LINK_TEXT_ARM
          : IDS_MAC_SYSTEM_INFOBAR_LINK_TEXT_ROSETTA);
}

GURL MacSystemInfoBarDelegate::GetLinkURL() const {
  return GURL(
      l10n_util::GetStringUTF8(GetEffectiveCPUType() == CPUType::kArm
                                   ? IDS_MAC_SYSTEM_INFOBAR_LINK_URL_ARM
                                   : IDS_MAC_SYSTEM_INFOBAR_LINK_URL_ROSETTA));
}

base::string16 MacSystemInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(GetEffectiveCPUType() == CPUType::kArm
                                       ? IDS_MAC_SYSTEM_INFOBAR_TEXT_ARM
                                       : IDS_MAC_SYSTEM_INFOBAR_TEXT_ROSETTA);
}

int MacSystemInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}
