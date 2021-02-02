// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_MAC_SYSTEM_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_STARTUP_MAC_SYSTEM_INFOBAR_DELEGATE_H_

#include "base/feature_list.h"
#include "base/strings/string16.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "url/gurl.h"

class InfoBarService;

// This infobar displays a message and link depending on the architecture of the
// Mac system. The message, link text, and link itself all come from grit, which
// allows them to be customized via field trials; placeholder values are present
// in the source grit files and marked as not translateable.
//
// In practice, this class will be used like this:
//   1. The MacSystemInfobar field trial is enabled
//   2. The "enable-arm" or "enable-rosetta" (or both) params for that trial
//      are set to "1"
//   3. The IDS_MAC_SYSTEM_INFOBAR_* grit messages are overridden with
//      appropriate text depending on the situation
//
// This allows the Mac-on-Arm launch process to "late bind" notification & help
// text in this infobar.
//
// The only logic in this infobar is for choosing between Arm string constants
// and Rosetta string constants. Once the field trial system becomes capable of
// qualifying study participants by architecture as well as OS version, this
// logic will go away as well, and this infobar will morph into a generic "show
// strings from a field trial whenever that field trial is active" type of
// infobar.
class MacSystemInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  static constexpr base::Feature kMacSystemInfoBar{
      "MacSystemInfoBar",
      base::FEATURE_DISABLED_BY_DEFAULT,
  };

  MacSystemInfoBarDelegate(const MacSystemInfoBarDelegate& other) = delete;
  MacSystemInfoBarDelegate& operator=(const MacSystemInfoBarDelegate& other) =
      delete;

  // Returns whether this infobar should show or not, depending on the current
  // CPU architecture and state of fieldtrials/switches.
  static bool ShouldShow();

  // Creates an instance of this infobar and adds it to the provided
  // InfoBarService. It is an error to call this method if !ShouldShow() outside
  // tests - the correct strings are not guaranteed to be produced.
  static void Create(InfoBarService* infobar_service);

 private:
  MacSystemInfoBarDelegate();
  ~MacSystemInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  bool ShouldExpire(const NavigationDetails& details) const override;
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  base::string16 GetLinkText() const override;
  GURL GetLinkURL() const override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
};

#endif  // CHROME_BROWSER_UI_STARTUP_MAC_SYSTEM_INFOBAR_DELEGATE_H_
