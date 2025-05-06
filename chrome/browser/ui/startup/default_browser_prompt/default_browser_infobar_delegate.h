// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_INFOBAR_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "chrome/browser/shell_integration.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

class Profile;

// The delegate for the infobar shown when Chrome is not the default browser.
// Ownership of the delegate is given to the infobar itself, the lifetime of
// which is bound to the containing WebContents.
class DefaultBrowserInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a default browser infobar and delegate and adds the infobar to
  // `infobar_manager`. If `can_pin_to_taskbar` is true, will attempt to
  // pin Chrome to the taskbar if the user clicks the set as default button.
  // This is currently only true on Windows, but it's possible this will
  // be extended to Mac.
  static infobars::InfoBar* Create(
      infobars::ContentInfoBarManager* infobar_manager,
      Profile* profile,
      bool can_pin_to_taskbar);

  DefaultBrowserInfoBarDelegate(const DefaultBrowserInfoBarDelegate&) = delete;
  DefaultBrowserInfoBarDelegate& operator=(
      const DefaultBrowserInfoBarDelegate&) = delete;

  DefaultBrowserInfoBarDelegate(base::PassKey<DefaultBrowserInfoBarDelegate>,
                                Profile* profile,
                                bool can_pin_to_taskbar);
  ~DefaultBrowserInfoBarDelegate() override;

 private:
  // Possible user interactions with the default browser info bar.
  // Do not modify the ordering as it is important for UMA.
  enum InfoBarUserInteraction {
    // The user clicked the "OK" (i.e., "Set as default") button.
    ACCEPT_INFO_BAR = 0,
    // The cancel button is deprecated.
    // CANCEL_INFO_BAR = 1,
    // The user did not interact with the info bar.
    IGNORE_INFO_BAR = 2,
    // The user explicitly closed the infobar.
    DISMISS_INFO_BAR = 3,
    NUM_INFO_BAR_USER_INTERACTION_TYPES
  };

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  void InfoBarDismissed() override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool ShouldHideInFullscreen() const override;

  // The WebContents's corresponding profile.
  raw_ptr<Profile> profile_;

  // If true, the info bar text will indicate that in addition to setting
  // the default browser, confirming the dialog will attempt to pin Chrome
  // to the taskbar (only ever true on Windows, currently).
  const bool can_pin_to_taskbar_ = false;

  // Indicates if the user interacted with the infobar.
  bool action_taken_ = false;
};

#endif  // CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_INFOBAR_DELEGATE_H_
