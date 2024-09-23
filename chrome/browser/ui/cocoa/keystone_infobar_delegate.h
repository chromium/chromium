// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_KEYSTONE_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_COCOA_KEYSTONE_INFOBAR_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

class PrefService;

namespace content {
class WebContents;
}

class KeystonePromotionInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  KeystonePromotionInfoBarDelegate(const KeystonePromotionInfoBarDelegate&) =
      delete;
  KeystonePromotionInfoBarDelegate& operator=(
      const KeystonePromotionInfoBarDelegate&) = delete;

  // Creates a keystone promotion delegate and adds it to the
  // infobars::ContentInfoBarManager associated with `web_contents`, if
  // `web_contents` is not nullptr.
  static void Create(content::WebContents* web_contents);

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
  raw_ptr<PrefService> prefs_;  // weak

  // Whether the info bar should be dismissed on the next navigation.
  bool can_expire_;

  // Used to delay the expiration of the info bar.
  base::WeakPtrFactory<KeystonePromotionInfoBarDelegate> weak_ptr_factory_;
};

// Shows an infobar asking the user to promote the updater to system scope,
// which requires authentication. The info bar will show in the most recently
// used Chrome tab. The info bar doesn't show if its "don't ask" button was
// ever clicked in the profile associated with that tab, nor if the "don't
// check default browser" command-line flag is present, nor if another info bar
// is already showing in the active tab.
void ShowUpdaterPromotionInfoBar();

#endif  // CHROME_BROWSER_UI_COCOA_KEYSTONE_INFOBAR_DELEGATE_H_
