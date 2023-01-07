// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_GOOGLE_API_KEYS_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_STARTUP_GOOGLE_API_KEYS_INFOBAR_DELEGATE_H_

#include <string>

#include "components/infobars/core/confirm_infobar_delegate.h"
#include "url/gurl.h"

namespace infobars {
class ContentInfoBarManager;
}

// An infobar that is run with a string and a "Learn More" link.
class GoogleApiKeysInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a missing Google API Keys infobar and delegate and adds the infobar
  // to |infobar_manager|.
  static void Create(infobars::ContentInfoBarManager* infobar_manager);

  GoogleApiKeysInfoBarDelegate(const GoogleApiKeysInfoBarDelegate&) = delete;
  GoogleApiKeysInfoBarDelegate& operator=(const GoogleApiKeysInfoBarDelegate&) =
      delete;

 private:
  GoogleApiKeysInfoBarDelegate();
  ~GoogleApiKeysInfoBarDelegate() override = default;

  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  std::u16string GetLinkText() const override;
  GURL GetLinkURL() const override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
};

#endif  // CHROME_BROWSER_UI_STARTUP_GOOGLE_API_KEYS_INFOBAR_DELEGATE_H_
