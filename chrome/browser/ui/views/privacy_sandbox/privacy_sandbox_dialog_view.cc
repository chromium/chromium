// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/privacy_sandbox/privacy_sandbox_dialog_view.h"

#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/privacy_sandbox/privacy_sandbox_prompt.h"
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

class PrivacySandboxDialogDelegate : public views::DialogDelegate {
 public:
  explicit PrivacySandboxDialogDelegate(Browser* browser) : browser_(browser) {
    if (auto* privacy_sandbox_serivce =
            PrivacySandboxServiceFactory::GetForProfile(browser->profile())) {
      privacy_sandbox_serivce->DialogOpenedForBrowser(browser);
    }
    SetCloseCallback(base::BindOnce(&PrivacySandboxDialogDelegate::OnClose,
                                    base::Unretained(this)));
  }

  bool OnCloseRequested(views::Widget::ClosedReason close_reason) override {
    // Only an unspecified close reason, which only occurs when the user has
    // actually made a choice is sufficient to close the consent. Reason for
    // closing the dialog (like dismissing notice dialog with escape) is handled
    // in WebUI.
    return close_reason == views::Widget::ClosedReason::kUnspecified;
  }

  void OnClose() {
    if (auto* privacy_sandbox_serivce =
            PrivacySandboxServiceFactory::GetForProfile(browser_->profile())) {
      privacy_sandbox_serivce->DialogClosedForBrowser(browser_);
    }
  }

 private:
  raw_ptr<Browser> browser_;
};

}  // namespace

// static
void ShowPrivacySandboxDialog(Browser* browser,
                              PrivacySandboxService::PromptType prompt_type) {
  auto delegate = std::make_unique<PrivacySandboxDialogDelegate>(browser);
  delegate->SetButtons(ui::DIALOG_BUTTON_NONE);
  delegate->SetModalType(ui::MODAL_TYPE_WINDOW);
  delegate->SetShowCloseButton(false);
  delegate->SetOwnedByWidget(true);

  delegate->SetContentsView(
      std::make_unique<PrivacySandboxDialogView>(browser, prompt_type));
  constrained_window::CreateBrowserModalDialogViews(
      std::move(delegate), browser->window()->GetNativeWindow());
}

PrivacySandboxDialogView::PrivacySandboxDialogView(
    Browser* browser,
    PrivacySandboxService::PromptType prompt_type)
    : browser_(browser) {
  // Create the web view in the native bubble.
  dialog_created_time_ = base::TimeTicks::Now();
  web_view_ =
      AddChildView(std::make_unique<views::WebView>(browser->profile()));
  web_view_->LoadInitialURL(GURL(chrome::kChromeUIPrivacySandboxDialogURL));

  auto width =
      views::LayoutProvider::Get()->GetSnappedDialogWidth(kDialogWidth);
  auto height = prompt_type == PrivacySandboxService::PromptType::kConsent
                    ? kDefaultConsentDialogHeight
                    : kDefaultNoticeDialogHeight;
  web_view_->SetPreferredSize(gfx::Size(width, height));

  PrivacySandboxDialogUI* web_ui = web_view_->GetWebContents()
                                       ->GetWebUI()
                                       ->GetController()
                                       ->GetAs<PrivacySandboxDialogUI>();
  DCHECK(web_ui);
  // Unretained is fine because this outlives the inner web UI.
  web_ui->Initialize(
      browser->profile(),
      base::BindOnce(&PrivacySandboxDialogView::Close, base::Unretained(this)),
      base::BindOnce(&PrivacySandboxDialogView::ResizeNativeView,
                     base::Unretained(this)),
      base::BindOnce(&PrivacySandboxDialogView::ShowNativeView,
                     base::Unretained(this)),
      base::BindOnce(&PrivacySandboxDialogView::OpenPrivacySandboxSettings,
                     base::Unretained(this)),
      prompt_type);

  SetUseDefaultFillLayout(true);
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
  GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());
}

void PrivacySandboxDialogView::ShowNativeView() {
  GetWidget()->Show();
  web_view_->RequestFocus();

  DCHECK(!dialog_created_time_.is_null());
  base::UmaHistogramTimes("Settings.PrivacySandbox.DialogLoadTime",
                          base::TimeTicks::Now() - dialog_created_time_);
}

void PrivacySandboxDialogView::OpenPrivacySandboxSettings() {
  DCHECK(browser_);
  chrome::ShowPrivacySandboxSettings(browser_);
}

BEGIN_METADATA(PrivacySandboxDialogView, views::View)
END_METADATA
