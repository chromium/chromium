// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TRUSTED_VAULT_TRUSTED_VAULT_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_WEBUI_TRUSTED_VAULT_TRUSTED_VAULT_DIALOG_DELEGATE_H_

#include <memory>
#include <string>

#include "content/public/browser/web_contents.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "url/gurl.h"

class Profile;

// Allows opening TrustedVault error pages (usually gaia reauth) in WebUI
// dialog.
class TrustedVaultDialogDelegate : public ui::WebDialogDelegate {
 public:
  // Used as an internal name for the widget corresponding to TrustedVault
  // reauth dialog. Exposed for testing.
  static constexpr char kWidgetName[] = "TrustedVaultReauthWidget";

  static void ShowDialogForProfile(Profile* profile, const GURL& url);

  explicit TrustedVaultDialogDelegate(
      std::unique_ptr<content::WebContents> contents,
      const GURL& url);
  ~TrustedVaultDialogDelegate() override;

 private:
  content::WebContents* web_contents() { return web_contents_.get(); }

  const std::unique_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_TRUSTED_VAULT_TRUSTED_VAULT_DIALOG_DELEGATE_H_
