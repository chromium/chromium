// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_BLOCKED_MIGRATION_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_BLOCKED_MIGRATION_INFOBAR_DELEGATE_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace infobars {
class InfoBar;
}

namespace web_app {

class WebAppBlockedMigrationInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates and adds the infobar to the given WebContents.
  static void Create(content::WebContents* web_contents,
                     base::OnceClosure on_dismiss_callback);

  // Removes the infobar from the given WebContents if it exists.
  static void Remove(content::WebContents* web_contents);

  // Finds the infobar in the given WebContents if it exists.
  static infobars::InfoBar* FindInfoBar(content::WebContents* web_contents);

  ~WebAppBlockedMigrationInfoBarDelegate() override;

  bool ShouldExpire(const NavigationDetails& details) const override;

  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  std::u16string GetMessageText() const override;
  std::u16string GetLinkText() const override;
  GURL GetLinkURL() const override;
  int GetButtons() const override;
  bool Accept() override;

  void InfoBarDismissed() override;

 private:
  explicit WebAppBlockedMigrationInfoBarDelegate(
      base::OnceClosure on_dismiss_callback);

  base::OnceClosure on_dismiss_callback_;
  const GURL learn_more_url_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_BLOCKED_MIGRATION_INFOBAR_DELEGATE_H_
