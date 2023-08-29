// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/trusted_vault/trusted_vault_dialog_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "base/location.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/trusted_vault/trusted_vault_encryption_keys_tab_helper.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

// Default size set to match signin reauth dialog size (see
// signin_view_controller_delegate_views.cc).
constexpr int kDefaultDialogHeight = 520;
constexpr int kDefaultDialogWidth = 540;

std::unique_ptr<content::WebContents> CreateWebContents(
    content::BrowserContext* context) {
  content::WebContents::CreateParams create_params(context, FROM_HERE);
  // Allows TrustedVault reauth page to close dialog using `window.close()`.
  // TODO(crbug.com/1434656): investigate whether reauth page can be changed to
  // close dialog either using TrustedVaultEncryptionKeysExtension (new method
  // needed) or other mechanism. Once this is done, this dialog can probably
  // reuse chrome::ShowWebDialog() and avoid controversy like line below.
  create_params.opened_by_another_window = true;
  return content::WebContents::Create(create_params);
}

}  // namespace

// static
void TrustedVaultDialogDelegate::ShowDialogForProfile(Profile* profile,
                                                      const GURL& url) {
  TrustedVaultDialogDelegate* dialog_delegate =
      new TrustedVaultDialogDelegate(url, profile);
  views::WebDialogView* view = new views::WebDialogView(
      profile, dialog_delegate, std::make_unique<ChromeWebContentsHandler>(),
      dialog_delegate->web_contents());

  views::Widget::InitParams params;
  params.delegate = view;
  params.name = kWidgetName;

  views::Widget* widget = new views::Widget;
  widget->Init(std::move(params));
  widget->Show();
}

TrustedVaultDialogDelegate::TrustedVaultDialogDelegate(const GURL& url,
                                                       Profile* profile)
    : url_(url), web_contents_(CreateWebContents(profile)) {
  TrustedVaultEncryptionKeysTabHelper::CreateForWebContents(
      web_contents_.get());
}

TrustedVaultDialogDelegate::~TrustedVaultDialogDelegate() = default;

ui::ModalType TrustedVaultDialogDelegate::GetDialogModalType() const {
  return ui::ModalType::MODAL_TYPE_NONE;
}

std::u16string TrustedVaultDialogDelegate::GetDialogTitle() const {
  return std::u16string();
}

GURL TrustedVaultDialogDelegate::GetDialogContentURL() const {
  return url_;
}

void TrustedVaultDialogDelegate::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {}

void TrustedVaultDialogDelegate::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDefaultDialogWidth, kDefaultDialogHeight);
}

std::string TrustedVaultDialogDelegate::GetDialogArgs() const {
  return std::string();
}

void TrustedVaultDialogDelegate::OnDialogShown(content::WebUI* webui) {}

void TrustedVaultDialogDelegate::OnDialogClosed(
    const std::string& json_retval) {
  delete this;
}

void TrustedVaultDialogDelegate::OnCloseContents(content::WebContents* source,
                                                 bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool TrustedVaultDialogDelegate::ShouldShowDialogTitle() const {
  return false;
}

bool TrustedVaultDialogDelegate::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // Disable context menu.
  return true;
}

content::WebContents* TrustedVaultDialogDelegate::web_contents() {
  return web_contents_.get();
}
