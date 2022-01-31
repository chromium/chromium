// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/privacy_sandbox/privacy_sandbox_dialog_view.h"

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/ui/privacy_sandbox/privacy_sandbox_dialog.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_dialog_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/fill_layout.h"

namespace {

// TODO(crbug.com/1286276) - Figure out the appropriate sizes.
constexpr int kDialogWidth = 500;
constexpr int kDialogHeight = 500;

}  // namespace

// static
void ShowPrivacySandboxDialog(Browser* browser,
                              PrivacySandboxService::DialogType dialog_type) {
  auto dialog =
      std::make_unique<PrivacySandboxDialogView>(browser, dialog_type);
  constrained_window::CreateBrowserModalDialogViews(
      std::move(dialog), browser->window()->GetNativeWindow())
      ->Show();
}

PrivacySandboxDialogView::PrivacySandboxDialogView(
    Browser* browser,
    PrivacySandboxService::DialogType dialog_type) {
  // Create the web view in the native bubble.
  auto* web_view =
      AddChildView(std::make_unique<views::WebView>(browser->profile()));
  web_view->LoadInitialURL(GURL(chrome::kChromeUIPrivacySandboxDialogURL));

  auto width =
      views::LayoutProvider::Get()->GetSnappedDialogWidth(kDialogWidth);
  web_view->SetPreferredSize({width, kDialogHeight});

  PrivacySandboxDialogUI* web_ui = web_view->GetWebContents()
                                       ->GetWebUI()
                                       ->GetController()
                                       ->GetAs<PrivacySandboxDialogUI>();
  SetInitiallyFocusedView(web_view);
  DCHECK(web_ui);
  // Unretained is fine because this outlives the inner web UI.
  web_ui->Initialize(
      browser->profile(),
      base::BindOnce(&PrivacySandboxDialogView::Close, base::Unretained(this)),
      dialog_type);

  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetModalType(ui::MODAL_TYPE_WINDOW);
  SetShowCloseButton(false);
  SetUseDefaultFillLayout(true);
}

void PrivacySandboxDialogView::Close() {
  GetWidget()->Close();
}

BEGIN_METADATA(PrivacySandboxDialogView, views::DialogDelegateView)
END_METADATA
