// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/privacy_sandbox/privacy_sandbox_dialog_view.h"

#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"
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
#include "net/base/url_util.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/layout/fill_layout.h"

namespace {

constexpr int kM1DialogWidth = 600;
constexpr int kDefaultDialogHeight = 494;
constexpr int kMinRequiredDialogHeight = 100;

GURL GetDialogURL(PrivacySandboxService::PromptType prompt_type) {
  GURL base_url = GURL(chrome::kChromeUIPrivacySandboxDialogURL);
  GURL combined_dialog_url =
      base_url.Resolve(chrome::kChromeUIPrivacySandboxDialogCombinedPath);
  switch (prompt_type) {
    case PrivacySandboxService::PromptType::kM1Consent:
      return combined_dialog_url;
    case PrivacySandboxService::PromptType::kM1NoticeROW:
      return base_url.Resolve(chrome::kChromeUIPrivacySandboxDialogNoticePath);
    case PrivacySandboxService::PromptType::kM1NoticeEEA:
      return net::AppendQueryParameter(combined_dialog_url, "step", "notice");
    case PrivacySandboxService::PromptType::kM1NoticeRestricted:
      return base_url.Resolve(
          chrome::kChromeUIPrivacySandboxDialogNoticeRestrictedPath);
    case PrivacySandboxService::PromptType::kNone:
      NOTREACHED();
  }
}

class PrivacySandboxDialogDelegate : public views::DialogDelegate {
 public:
  explicit PrivacySandboxDialogDelegate(Browser* browser) : browser_(browser) {
    RegisterWindowClosingCallback(
        base::BindOnce(&PrivacySandboxDialogDelegate::OnWindowClosing,
                       base::Unretained(this)));
  }

  bool OnCloseRequested(views::Widget::ClosedReason close_reason) override {
    // Only an unspecified close reason, which only occurs when the user has
    // actually made a choice is sufficient to close the consent. Reason for
    // closing the dialog (like dismissing notice dialog with escape) is handled
    // in WebUI.
    return close_reason == views::Widget::ClosedReason::kUnspecified;
  }

  void OnWindowClosing() {
    if (auto* privacy_sandbox_service =
            PrivacySandboxServiceFactory::GetForProfile(browser_->profile())) {
      privacy_sandbox_service->PromptClosedForBrowser(browser_);
    }
  }

 private:
  raw_ptr<Browser> browser_;
};

}  // namespace

// static
bool CanWindowHeightFitPrivacySandboxPrompt(Browser* browser) {
  const int max_dialog_height = browser->window()
                                    ->GetWebContentsModalDialogHost()
                                    ->GetMaximumDialogSize()
                                    .height();
  return max_dialog_height >= kMinRequiredDialogHeight;
}

// static
void ShowPrivacySandboxDialog(Browser* browser,
                              PrivacySandboxService::PromptType prompt_type) {
  auto delegate = std::make_unique<PrivacySandboxDialogDelegate>(browser);
  delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  delegate->SetModalType(ui::mojom::ModalType::kWindow);
  delegate->SetShowCloseButton(false);
  delegate->SetOwnedByWidget(true);

  delegate->SetContentsView(
      std::make_unique<PrivacySandboxDialogView>(browser, prompt_type));
  auto* widget = constrained_window::CreateBrowserModalDialogViews(
      std::move(delegate), browser->window()->GetNativeWindow());

  if (auto* privacy_sandbox_service =
          PrivacySandboxServiceFactory::GetForProfile(browser->profile())) {
    privacy_sandbox_service->PromptOpenedForBrowser(browser, widget);
  }
}

PrivacySandboxDialogView::PrivacySandboxDialogView(
    Browser* browser,
    PrivacySandboxService::PromptType prompt_type)
    : browser_(browser) {
  CHECK_NE(PrivacySandboxService::PromptType::kNone, prompt_type);
  // Create the web view in the native bubble.
  dialog_created_time_ = base::TimeTicks::Now();
  web_view_ =
      AddChildView(std::make_unique<views::WebView>(browser->profile()));
  web_view_->LoadInitialURL(GetDialogURL(prompt_type));

  // Override the default zoom level for the Privacy Sandbox dialog. Its size
  // should align with native UI elements, rather than web content.
  auto* web_contents = web_view_->GetWebContents();
  auto* rfh = web_contents->GetPrimaryMainFrame();
  auto* zoom_map = content::HostZoomMap::GetForWebContents(web_contents);
  zoom_map->SetTemporaryZoomLevel(rfh->GetGlobalId(),
                                  blink::ZoomFactorToZoomLevel(1.0f));

  const int max_width = browser_->window()
                            ->GetWebContentsModalDialogHost()
                            ->GetMaximumDialogSize()
                            .width();
  const int width =
      views::LayoutProvider::Get()->GetSnappedDialogWidth(kM1DialogWidth);
  web_view_->SetPreferredSize(
      gfx::Size(std::min(width, max_width), kDefaultDialogHeight));

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
      base::BindOnce(
          &PrivacySandboxDialogView::OpenPrivacySandboxAdMeasurementSettings,
          base::Unretained(this)),
      prompt_type);

  SetUseDefaultFillLayout(true);
}

void PrivacySandboxDialogView::Close() {
  GetWidget()->Close();
}

void PrivacySandboxDialogView::ResizeNativeView(int height) {
  const int max_height = browser_->window()
                             ->GetWebContentsModalDialogHost()
                             ->GetMaximumDialogSize()
                             .height();
  const int target_height = std::min(height, max_height);
  web_view_->SetPreferredSize(
      gfx::Size(web_view_->GetPreferredSize().width(), target_height));
  constrained_window::UpdateWebContentsModalDialogPosition(
      GetWidget(), browser_->window()->GetWebContentsModalDialogHost());
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

void PrivacySandboxDialogView::OpenPrivacySandboxAdMeasurementSettings() {
  CHECK(browser_);
  chrome::ShowPrivacySandboxAdMeasurementSettings(browser_);
}

content::WebContents* PrivacySandboxDialogView::GetWebContentsForTesting() {
  CHECK(web_view_);
  return web_view_->GetWebContents();
}

BEGIN_METADATA(PrivacySandboxDialogView)
END_METADATA
