// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_BLOCKED_MIGRATION_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_BLOCKED_MIGRATION_INFOBAR_DELEGATE_H_

#include "components/infobars/core/confirm_infobar_delegate.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace web_app {

class WebAppBlockedMigrationInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates and adds the infobar to the given WebContents.
  static void Create(content::WebContents* web_contents);

  ~WebAppBlockedMigrationInfoBarDelegate() override;

  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  std::u16string GetMessageText() const override;
  std::u16string GetLinkText() const override;
  GURL GetLinkURL() const override;
  int GetButtons() const override;

 private:
  explicit WebAppBlockedMigrationInfoBarDelegate();

  const GURL learn_more_url_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_BLOCKED_MIGRATION_INFOBAR_DELEGATE_H_
