// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_STARTUP_LAUNCH_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_STARTUP_STARTUP_LAUNCH_INFOBAR_DELEGATE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "chrome/browser/startup/startup_launch_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "ui/base/ui_base_types.h"

class Profile;

// A delegate for the "Start Chrome with Windows" infobar.
// This infobar asks the user if they want to enable the feature (OptIn)
// or allows them to configure it (OptOut).
class StartupLaunchInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  static std::unique_ptr<StartupLaunchInfoBarDelegate> Create(
      Profile* profile,
      StartupLaunchInfoBarManager::InfoBarType infobar_type);

  StartupLaunchInfoBarDelegate(const StartupLaunchInfoBarDelegate&);
  StartupLaunchInfoBarDelegate& operator=(const StartupLaunchInfoBarDelegate&) =
      delete;

  StartupLaunchInfoBarDelegate(
      base::PassKey<StartupLaunchInfoBarDelegate>,
      Profile* profile,
      StartupLaunchInfoBarManager::InfoBarType infobar_type);
  ~StartupLaunchInfoBarDelegate() override;

 private:
  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool ShouldHideInFullscreen() const override;
  bool Accept() override;
  std::optional<ui::ButtonStyle> GetButtonStyle(
      ConfirmInfoBarDelegate::InfoBarButton button) const override;

  // The WebContents's corresponding profile.
  raw_ptr<Profile> profile_;
  StartupLaunchInfoBarManager::InfoBarType infobar_type_;
};

#endif  // CHROME_BROWSER_UI_STARTUP_STARTUP_LAUNCH_INFOBAR_DELEGATE_H_
