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
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

// Default size set to match signin reauth dialog size (see
// signin_view_controller_delegate_views.cc).
constexpr gfx::Size kDefaultSize{520, 540};

std::unique_ptr<content::WebContents> CreateWebContents(
    content::BrowserContext* context) {
  content::WebContents::CreateParams create_params(context, FROM_HERE);
  // Allows TrustedVault reauth page to close dialog using `window.close()`.
  // TODO(crbug.com/40264837): investigate whether reauth page can be changed to
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
  auto dialog_delegate = std::make_unique<TrustedVaultDialogDelegate>(
      CreateWebContents(profile), url);
  content::WebContents* contents = dialog_delegate->web_contents();
  views::WebDialogView* view = new views::WebDialogView(
      profile, dialog_delegate.release(),
      std::make_unique<ChromeWebContentsHandler>(), contents);

  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  params.delegate = view;
  params.name = kWidgetName;

  views::Widget* widget = new views::Widget;
  widget->Init(std::move(params));
  widget->Show();
}

TrustedVaultDialogDelegate::TrustedVaultDialogDelegate(
    std::unique_ptr<content::WebContents> contents,
    const GURL& url)
    : web_contents_(std::move(contents)) {
  set_allow_default_context_menu(false);
  set_can_close(true);
  set_dialog_content_url(url);
  set_dialog_modal_type(ui::mojom::ModalType::kNone);
  set_dialog_size(kDefaultSize);
  set_show_dialog_title(false);
  TrustedVaultEncryptionKeysTabHelper::CreateForWebContents(
      web_contents_.get());
}

TrustedVaultDialogDelegate::~TrustedVaultDialogDelegate() = default;
