// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/pin_infobar/pin_infobar_delegate.h"

#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/win/taskbar_manager.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/startup/default_browser_prompt/pin_infobar/pin_infobar_mac_util.h"
#endif  // BUILDFLAG(IS_MAC)

namespace default_browser {

namespace {

void RecordUserInteractionHistogram(PinInfoBarUserInteraction interaction) {
  base::UmaHistogramEnumeration("DefaultBrowser.PinInfoBar.UserInteraction",
                                interaction);
}

struct ExperimentalString {
  std::u16string_view message_win;
  std::u16string_view button_win;
  std::u16string_view message_mac;
  std::u16string_view button_mac;
};

const ExperimentalString kExperimentalStrings[] = {
    {u"", u"", u"", u""},  // Version 0 (Standard)
    {u"Open Chrome with one click. Pin it for faster access.",
     u"Enable One-Click Access",
     u"Open Chrome with one click. Keep it in your Dock for faster access.",
     u"Enable One-Click Access"},
    {u"Launch the web faster. Pin Chrome to your taskbar.", u"Pin for Speed",
     u"Launch the web faster. Keep Chrome in your Dock.", u"Keep in Dock"},
    {u"Keep Chrome within reach anytime you browse.", u"Pin Chrome Now",
     u"Keep Chrome within reach anytime you browse.", u"Keep in Dock Now"},
    {u"Optimize your workflow. Add Chrome to your taskbar.", u"Add to Taskbar",
     u"Optimize your workflow. Add Chrome to your Dock.", u"Add to Dock"},
    {u"Never search for your browser again. Secure it here.",
     u"Secure to Taskbar",
     u"Never search for your browser again. Secure it here.",
     u"Secure to Dock"},
    {u"Simplify your startup. Pin Chrome to your taskbar.", u"Pin to Taskbar",
     u"Simplify your startup. Keep Chrome in your Dock.", u"Keep in Dock"},
    {u"Always one click away from the web.", u"Pin to Taskbar",
     u"Always one click away from the web.", u"Keep in Dock"},
    {u"Ready when you are.", u"Pin Chrome", u"Ready when you are.",
     u"Keep in Dock"},
    {u"For a better desktop experience, pin Chrome to your taskbar.",
     u"Pin to Taskbar",
     u"For a better desktop experience, keep Chrome in your Dock.",
     u"Keep in Dock"},
    {u"Start browsing instantly. Pin Chrome to your taskbar.", u"Pin Chrome",
     u"Start browsing instantly. Keep Chrome in your Dock.", u"Keep in Dock"}};

}  // namespace

// static
infobars::InfoBar* PinInfoBarDelegate::Create(
    infobars::ContentInfoBarManager* infobar_manager) {
  CHECK(infobar_manager);
  return infobar_manager->AddInfoBar(
      CreateConfirmInfoBar(std::make_unique<PinInfoBarDelegate>()));
}

PinInfoBarDelegate::~PinInfoBarDelegate() {
  if (!action_taken_) {
    RecordUserInteractionHistogram(PinInfoBarUserInteraction::kIgnored);
  }
}

infobars::InfoBarDelegate::InfoBarIdentifier PinInfoBarDelegate::GetIdentifier()
    const {
  return PIN_INFOBAR_DELEGATE;
}

const gfx::VectorIcon& PinInfoBarDelegate::GetVectorIcon() const {
  return dark_mode() ? omnibox::kProductChromeRefreshIcon
                     : vector_icons::kProductRefreshIcon;
}

std::u16string PinInfoBarDelegate::GetMessageText() const {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  if (base::FeatureList::IsEnabled(features::kSeparateDefaultAndPinPrompt)) {
    const int version =
        features::kSeparateDefaultAndPinPromptMessageVersion.Get();
    auto experimental_strings = base::span(kExperimentalStrings);
    if (version >= 1 &&
        static_cast<size_t>(version) < experimental_strings.size()) {
#if BUILDFLAG(IS_WIN)
      return std::u16string(experimental_strings[version].message_win);
#else
      return std::u16string(experimental_strings[version].message_mac);
#endif
    }
  }
#endif

#if BUILDFLAG(IS_WIN)
  return l10n_util::GetStringUTF16(IDS_PIN_INFOBAR_TEXT);
#elif BUILDFLAG(IS_MAC)
  return l10n_util::GetStringUTF16(IDS_PIN_INFOBAR_DOCK_TEXT);
#endif
}

std::u16string PinInfoBarDelegate::GetButtonLabel(InfoBarButton button) const {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  if (base::FeatureList::IsEnabled(features::kSeparateDefaultAndPinPrompt)) {
    const int version =
        features::kSeparateDefaultAndPinPromptMessageVersion.Get();
    auto experimental_strings = base::span(kExperimentalStrings);
    if (version >= 1 &&
        static_cast<size_t>(version) < experimental_strings.size()) {
#if BUILDFLAG(IS_WIN)
      return std::u16string(experimental_strings[version].button_win);
#else
      return std::u16string(experimental_strings[version].button_mac);
#endif
    }
  }
#endif

#if BUILDFLAG(IS_WIN)
  return l10n_util::GetStringUTF16(IDS_PIN_INFOBAR_BUTTON);
#elif BUILDFLAG(IS_MAC)
  return l10n_util::GetStringUTF16(IDS_PIN_INFOBAR_DOCK_BUTTON);
#endif
}

int PinInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

bool PinInfoBarDelegate::Accept() {
  action_taken_ = true;
  RecordUserInteractionHistogram(PinInfoBarUserInteraction::kAccepted);

  // Pin Chrome to taskbar.
#if BUILDFLAG(IS_WIN)
  browser_util::PinAppToTaskbar(
      ShellUtil::GetBrowserModelId(InstallUtil::IsPerUserInstall()),
      browser_util::PinAppToTaskbarChannel::kPinToTaskbarInfoBar,
      base::DoNothing());
#elif BUILDFLAG(IS_MAC)
  PinChromeToDock();
#endif

  return ConfirmInfoBarDelegate::Accept();
}

void PinInfoBarDelegate::InfoBarDismissed() {
  action_taken_ = true;
  RecordUserInteractionHistogram(PinInfoBarUserInteraction::kDismissed);
  ConfirmInfoBarDelegate::InfoBarDismissed();
}

}  // namespace default_browser
