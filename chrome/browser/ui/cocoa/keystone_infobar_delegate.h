// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_KEYSTONE_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_COCOA_KEYSTONE_INFOBAR_DELEGATE_H_

#include "base/memory/weak_ptr.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

class PrefService;
class Profile;

namespace content {
class WebContents;
}

class KeystonePromotionInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a keystone promotion delegate and adds it to the InfoBarService
  // associated with |webContents|.
  static void Create(content::WebContents* webContents);

 private:
  explicit KeystonePromotionInfoBarDelegate(PrefService* prefs);
  ~KeystonePromotionInfoBarDelegate() override;

  // Sets this info bar to be able to expire.  Called a predetermined amount
  // of time after this object is created.
  void SetCanExpire() { can_expire_ = true; }

  // ConfirmInfoBarDelegate
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  std::u16string GetMessageText() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

  // The prefs to use.
  PrefService* prefs_;  // weak

  // Whether the info bar should be dismissed on the next navigation.
  bool can_expire_;

  // Used to delay the expiration of the info bar.
  base::WeakPtrFactory<KeystonePromotionInfoBarDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(KeystonePromotionInfoBarDelegate);
};

class KeystoneInfoBar {
 public:
  // If the application is Keystone-enabled and not on a read-only filesystem
  // (capable of being auto-updated), and Keystone indicates that it needs
  // ticket promotion, PromotionInfoBar displays an info bar asking the user
  // to promote the ticket.  The user will need to authenticate in order to
  // gain authorization to perform the promotion.  The info bar is not shown
  // if its "don't ask" button was ever clicked, if the "don't check default
  // browser" command-line flag is present, on the very first launch, or if
  // another info bar is already showing in the active tab.
  static void PromotionInfoBar(Profile* profile);
};

#endif  // CHROME_BROWSER_UI_COCOA_KEYSTONE_INFOBAR_DELEGATE_H_
