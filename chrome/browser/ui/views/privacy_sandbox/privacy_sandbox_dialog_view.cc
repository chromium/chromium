// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/privacy_sandbox/privacy_sandbox_dialog_view.h"

#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom-forward.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
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

using enum PrivacySandboxService::AdsDialogCallbackNoArgsEvents;
using privacy_sandbox::notice::mojom::PrivacySandboxNotice;
using views::WebView;

constexpr int kM1DialogWidth = 600;
constexpr int kDefaultDialogHeight = 494;

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
  explicit PrivacySandboxDialogDelegate(BrowserWindowInterface* browser)
      : browser_(browser) {
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
    // TODO(crbug.com/408016824): To be deprecated once V2 is migrated to.
    if (auto* privacy_sandbox_service =
            PrivacySandboxServiceFactory::GetForProfile(
                browser_->GetProfile())) {
      privacy_sandbox_service->PromptClosedForBrowser(browser_);
    }
  }

 private:
  raw_ptr<BrowserWindowInterface> browser_;
};

}  // namespace

// static
void PrivacySandboxDialog::Show(Browser* browser,
                                PrivacySandboxService::PromptType prompt_type) {
  auto delegate = std::make_unique<PrivacySandboxDialogDelegate>(browser);
  delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  delegate->SetModalType(ui::mojom::ModalType::kWindow);
  delegate->SetShowCloseButton(false);
  delegate->SetOwnedByWidget(views::WidgetDelegate::OwnedByWidgetPassKey());

  auto dialog_view = PrivacySandboxDialogView::CreateDialogViewForPromptType(
      browser, prompt_type);
  delegate->SetContentsView(std::move(dialog_view));

  auto* widget = constrained_window::CreateBrowserModalDialogViews(
      std::move(delegate), browser->window()->GetNativeWindow());

  if (auto* privacy_sandbox_service =
          PrivacySandboxServiceFactory::GetForProfile(browser->profile())) {
    privacy_sandbox_service->PromptOpenedForBrowser(browser, widget);
  }
}

PrivacySandboxDialogView::PrivacySandboxDialogView(
    BrowserWindowInterface* browser)
    : web_view_(AddChildView(std::make_unique<WebView>(browser->GetProfile()))),
      browser_(browser) {
  // Override the default zoom level for the Privacy Sandbox dialog. Its size
  // should align with native UI elements, rather than web content.
  auto* web_contents = web_view_->GetWebContents();
  auto* rfh = web_contents->GetPrimaryMainFrame();
  auto* zoom_map = content::HostZoomMap::GetForWebContents(web_contents);
  zoom_map->SetTemporaryZoomLevel(rfh->GetGlobalId(),
                                  blink::ZoomFactorToZoomLevel(1.0f));

  const int max_width = browser->GetWebContentsModalDialogHostForWindow()
                            ->GetMaximumDialogSize()
                            .width();
  const int width =
      views::LayoutProvider::Get()->GetSnappedDialogWidth(kM1DialogWidth);
  web_view_->SetPreferredSize(
      gfx::Size(std::min(width, max_width), kDefaultDialogHeight));
}

// static
std::unique_ptr<PrivacySandboxDialogView>
PrivacySandboxDialogView::CreateDialogViewForPromptType(
    BrowserWindowInterface* browser,
    PrivacySandboxService::PromptType prompt_type) {
  CHECK_NE(PrivacySandboxService::PromptType::kNone, prompt_type);

  // Sets content view.
  // Using `new` to access a non-public constructor.
  auto dialog_view = base::WrapUnique(new PrivacySandboxDialogView(browser));
  dialog_view->InitializeDialogUIForPromptType(prompt_type);
  return dialog_view;
}

void PrivacySandboxDialogView::InitializeDialogUIForPromptType(
    PrivacySandboxService::PromptType prompt_type) {
  web_view_->LoadInitialURL(GetDialogURL(prompt_type));
  PrivacySandboxDialogUI* web_ui = web_view_->GetWebContents()
                                       ->GetWebUI()
                                       ->GetController()
                                       ->GetAs<PrivacySandboxDialogUI>();
  DCHECK(web_ui);
  // Unretained is fine because this outlives the inner web UI.
  web_ui->Initialize(
      browser_->GetProfile(),
      base::BindRepeating(&PrivacySandboxDialogView::AdsDialogNoArgsCallback,
                          base::Unretained(this)),
      base::BindOnce(&PrivacySandboxDialogView::ResizeNativeView,
                     base::Unretained(this)),

      prompt_type);

  SetUseDefaultFillLayout(true);
}

void PrivacySandboxDialogView::AdsDialogNoArgsCallback(
    PrivacySandboxService::AdsDialogCallbackNoArgsEvents event) {
  switch (event) {
    case kShowDialog:
      ShowNativeView();
      break;
    case kCloseDialog:
      CloseNativeView();
      break;
    case kOpenAdsPrivacySettings:
      OpenPrivacySandboxSettings();
      break;
    case kOpenMeasurementSettings:
      OpenPrivacySandboxAdMeasurementSettings();
      break;
  }
}

void PrivacySandboxDialogView::CloseNativeView() {
  GetWidget()->Close();
}

BrowserWindowInterface* PrivacySandboxDialogView::GetBrowser() {
  return browser_;
}

void PrivacySandboxDialogView::ResizeNativeView(int height) {
  const int max_height = browser_->GetWebContentsModalDialogHostForWindow()
                             ->GetMaximumDialogSize()
                             .height();
  const int target_height = std::min(height, max_height);
  web_view_->SetPreferredSize(
      gfx::Size(web_view_->GetPreferredSize().width(), target_height));
  constrained_window::UpdateWebContentsModalDialogPosition(
      GetWidget(), browser_->GetWebContentsModalDialogHostForWindow());
}

void PrivacySandboxDialogView::ShowNativeView(
    base::OnceCallback<void()> view_shown_callback) {
  GetWidget()->Show();
  web_view_->RequestFocus();
  std::move(view_shown_callback).Run();
}

void PrivacySandboxDialogView::OpenPrivacySandboxSettings() {
  DCHECK(browser_);
  chrome::ShowPrivacySandboxSettings(browser_->GetBrowserForMigrationOnly());
}

void PrivacySandboxDialogView::OpenPrivacySandboxAdMeasurementSettings() {
  CHECK(browser_);
  chrome::ShowPrivacySandboxAdMeasurementSettings(
      browser_->GetBrowserForMigrationOnly());
}

content::WebContents* PrivacySandboxDialogView::GetWebContentsForTesting() {
  CHECK(web_view_);
  return web_view_->GetWebContents();
}

BEGIN_METADATA(PrivacySandboxDialogView)
END_METADATA
