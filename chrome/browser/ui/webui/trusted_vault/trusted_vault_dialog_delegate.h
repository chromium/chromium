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

  TrustedVaultDialogDelegate(const TrustedVaultDialogDelegate&) = delete;
  TrustedVaultDialogDelegate& operator=(const TrustedVaultDialogDelegate&) =
      delete;
  ~TrustedVaultDialogDelegate() override;

  // WebDialogDelegate
  ui::ModalType GetDialogModalType() const override;
  std::u16string GetDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  void OnDialogShown(content::WebUI* webui) override;
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool ShouldShowDialogTitle() const override;
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;

 private:
  explicit TrustedVaultDialogDelegate(const GURL& url, Profile* profile);

  content::WebContents* web_contents();

  const GURL url_;
  const std::unique_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_TRUSTED_VAULT_TRUSTED_VAULT_DIALOG_DELEGATE_H_
