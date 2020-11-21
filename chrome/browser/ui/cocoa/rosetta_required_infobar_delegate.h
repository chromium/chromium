// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_ROSETTA_REQUIRED_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_COCOA_ROSETTA_REQUIRED_INFOBAR_DELEGATE_H_

#include "base/strings/string16.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

// This infobar displays a message that Rosetta is required to play protected
// media, and initiates an installation of Rosetta if the user wants.
class RosettaRequiredInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  RosettaRequiredInfoBarDelegate();
  ~RosettaRequiredInfoBarDelegate() override;
  RosettaRequiredInfoBarDelegate(const RosettaRequiredInfoBarDelegate& other) =
      delete;
  RosettaRequiredInfoBarDelegate& operator=(
      const RosettaRequiredInfoBarDelegate& other) = delete;

  // Returns whether this infobar should show or not.
  static bool ShouldShow();

  // Creates an instance of this infobar and adds it to the provided
  // WebContents. It is an error to call this method if !ShouldShow() outside
  // tests - the correct strings are not guaranteed to be produced.
  static void Create(content::WebContents* web_contents);

 private:
  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  base::string16 GetLinkText() const override;
  GURL GetLinkURL() const override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
};

#endif  // CHROME_BROWSER_UI_COCOA_ROSETTA_REQUIRED_INFOBAR_DELEGATE_H_
