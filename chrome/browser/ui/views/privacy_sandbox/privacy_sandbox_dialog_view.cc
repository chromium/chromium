// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/privacy_sandbox/privacy_sandbox_dialog_view.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/privacy_sandbox/privacy_sandbox_dialog.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_dialog_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/fill_layout.h"

namespace {

constexpr int kDialogWidth = 512;
constexpr int kDefaultConsentDialogHeight = 569;
constexpr int kDefaultNoticeDialogHeight = 494;

}  // namespace

// static
void ShowPrivacySandboxDialog(Browser* browser,
                              PrivacySandboxService::DialogType dialog_type) {
  auto dialog =
      std::make_unique<PrivacySandboxDialogView>(browser, dialog_type);
  constrained_window::CreateBrowserModalDialogViews(
      std::move(dialog), browser->window()->GetNativeWindow());
}

PrivacySandboxDialogView::PrivacySandboxDialogView(
    Browser* browser,
    PrivacySandboxService::DialogType dialog_type)
    : browser_(browser) {
  // Create the web view in the native bubble.
  dialog_created_time_ = base::TimeTicks::Now();
  web_view_ =
      AddChildView(std::make_unique<views::WebView>(browser->profile()));
  web_view_->LoadInitialURL(GURL(chrome::kChromeUIPrivacySandboxDialogURL));

  auto width =
      views::LayoutProvider::Get()->GetSnappedDialogWidth(kDialogWidth);
  auto height = dialog_type == PrivacySandboxService::DialogType::kConsent
                    ? kDefaultConsentDialogHeight
                    : kDefaultNoticeDialogHeight;
  web_view_->SetPreferredSize({width, height});

  PrivacySandboxDialogUI* web_ui = web_view_->GetWebContents()
                                       ->GetWebUI()
                                       ->GetController()
                                       ->GetAs<PrivacySandboxDialogUI>();
  SetInitiallyFocusedView(web_view_);
  DCHECK(web_ui);
  // Unretained is fine because this outlives the inner web UI.
  web_ui->Initialize(
      browser->profile(),
      base::BindOnce(&PrivacySandboxDialogView::Close, base::Unretained(this)),
      base::BindOnce(&PrivacySandboxDialogView::ResizeNativeView,
                     base::Unretained(this)),
      base::BindOnce(&PrivacySandboxDialogView::OpenPrivacySandboxSettings,
                     base::Unretained(this)),
      dialog_type);

  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetModalType(ui::MODAL_TYPE_WINDOW);
  SetShowCloseButton(false);
  SetUseDefaultFillLayout(true);
  set_margins(gfx::Insets());
}

void PrivacySandboxDialogView::Close() {
  GetWidget()->Close();
}

void PrivacySandboxDialogView::ResizeNativeView(int height) {
  int max_height = browser_->window()
                       ->GetWebContentsModalDialogHost()
                       ->GetMaximumDialogSize()
                       .height();
  web_view_->SetPreferredSize(gfx::Size(web_view_->GetPreferredSize().width(),
                                        std::min(height, max_height)));
  SizeToContents();
  GetWidget()->Show();

  DCHECK(!dialog_created_time_.is_null());
  base::UmaHistogramTimes("Settings.PrivacySandbox.DialogLoadTime",
                          base::TimeTicks::Now() - dialog_created_time_);
}

void PrivacySandboxDialogView::OpenPrivacySandboxSettings() {
  DCHECK(browser_);
  chrome::ShowPrivacySandboxSettings(browser_);
}

BEGIN_METADATA(PrivacySandboxDialogView, views::BubbleDialogDelegateView)
END_METADATA
