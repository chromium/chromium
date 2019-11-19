// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/hats/hats_web_dialog.h"

#include <algorithm>

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/chrome_web_dialog_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/hats/hats_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/browser_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/template_expressions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"
#include "url/url_canon.h"
#include "url/url_util.h"

namespace {

// Default width/height of the dialog in screen size.
const int kDefaultHatsDialogWidth = 448;
const int kDefaultHatsDialogHeight = 440;

// Placeholder string in html file to be replaced when the file is loaded.
constexpr char kScriptSrcReplacementToken[] = "scriptSrc";

// Google consumer survey host site.
constexpr char kSurveyHost[] =
    "https://www.google.com/insights/consumersurveys";
// Base URL to fetch the Google consumer survey script.
constexpr char kBaseFormatUrl[] = "%s/async_survey?site=%s&force_https=1&sc=%s";

// Returns the local HaTS HTML file as a string with the correct Hats script
// URL.
// |site_id| refers to the 'Site ID' used by HaTS server to uniquely identify
// a survey to be served.
// |site_context| is the extra information that may be sent along with the
// survey feedback to the HaTS server.
std::string LoadLocalHtmlAsString(const std::string& site_id,
                                  const std::string& site_context) {
  base::StringPiece raw_html =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_DESKTOP_HATS_HTML);
  ui::TemplateReplacements replacements;
  replacements[kScriptSrcReplacementToken] = base::StringPrintf(
      kBaseFormatUrl, kSurveyHost, site_id.c_str(), site_context.c_str());
  return ui::ReplaceTemplateExpressions(raw_html, replacements);
}

}  // namespace

// static
void HatsWebDialog::Create(Browser* browser, const std::string& site_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(browser);

  // Self deleting upon close.
  auto* hats_dialog = new HatsWebDialog(browser, site_id);
  hats_dialog->CreateWebDialog(browser);
}

HatsWebDialog::HatsWebDialog(Browser* browser, const std::string& site_id)
    : otr_profile_registration_(
          IndependentOTRProfileManager::GetInstance()
              ->CreateFromOriginalProfile(
                  browser->profile(),
                  base::BindOnce(&HatsWebDialog::OnOriginalProfileDestroyed,
                                 base::Unretained(this)))),
      browser_(browser),
      site_id_(site_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(
      otr_profile_registration_->profile()->IsIndependentOffTheRecordProfile());
}

HatsWebDialog::~HatsWebDialog() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ui::ModalType HatsWebDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

base::string16 HatsWebDialog::GetDialogTitle() const {
  return base::string16();
}

GURL HatsWebDialog::GetDialogContentURL() const {
  // Load the html data and use it in a data URL to be displayed in the dialog.
  std::string url_str =
      LoadLocalHtmlAsString(site_id_, version_info::GetVersionNumber());
  url::RawCanonOutputT<char> url;
  url::EncodeURIComponent(url_str.c_str(), url_str.length(), &url);
  return GURL("data:text/html;charset=utf-8," +
              std::string(url.data(), url.length()));
}

void HatsWebDialog::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {}

void HatsWebDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDefaultHatsDialogWidth, kDefaultHatsDialogHeight);
}

bool HatsWebDialog::CanResizeDialog() const {
  return false;
}

std::string HatsWebDialog::GetDialogArgs() const {
  return std::string();
}

void HatsWebDialog::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void HatsWebDialog::OnCloseContents(content::WebContents* source,
                                    bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool HatsWebDialog::ShouldShowDialogTitle() const {
  return false;
}

bool HatsWebDialog::HandleContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  // Disable context menu.
  return true;
}

void HatsWebDialog::OnWebContentsFinishedLoad() {
  // If this happens after the time out, the dialog is to be deleted. We
  // should not handle this any more.
  if (!loading_timer_.IsRunning())
    return;
  loading_timer_.Stop();

  if (resource_loaded_) {
    // Only after the confirmation that we have already loaded the resource, we
    // will proceed with showing the bubble.
    HatsBubbleView::Show(browser_, base::BindOnce(&HatsWebDialog::Show,
                                                  weak_factory_.GetWeakPtr(),
                                                  preloading_widget_));
  } else {
    preloading_widget_->Close();
  }
}

void HatsWebDialog::OnMainFrameResourceLoadComplete(
    const content::mojom::ResourceLoadInfo& resource_load_info) {
  // Due to https://crbug.com/1011433, we don't always get called due to failed
  // loading for javascript resource. So, We monitor all the resource load,
  // and explicitly HaTS library code. We only claim |resource_loaded_| is true
  // if HaTS library code is loaded successfully, thus this function is called,
  // and there isn't any error for loading other resources.
  // TODO(weili): once the bug is fixed, we no longer need this check nor
  // |resource_loaded_|, remove them then.
  if (resource_load_info.net_error == net::Error::OK) {
    if (resource_load_info.url.spec().find(kSurveyHost) == 0) {
      // The resource from survey host is loaded successfully.
      resource_loaded_ = true;
    }
  } else {
    // Any error indicates some resource failed to load. Exit early.
    loading_timer_.Stop();
    preloading_widget_->Close();
  }
}

void HatsWebDialog::OnLoadTimedOut() {
  // Once loading is timed out, it means there is some problem such as network
  // error, unresponsive server etc. No need to wait any longer. Delete the
  // dialog.
  preloading_widget_->Close();
}

const base::TimeDelta HatsWebDialog::ContentLoadingTimeout() const {
  // Time out for loading the survey content in the dialog.
  static const base::TimeDelta kLoadingTimeOut =
      base::TimeDelta::FromSeconds(10);

  return kLoadingTimeOut;
}

void HatsWebDialog::CreateWebDialog(Browser* browser) {
  // Create a web dialog aligned to the bottom center of the location bar.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  LocationBarView* location_bar = browser_view->GetLocationBarView();
  DCHECK(location_bar);
  gfx::Rect bounds(location_bar->bounds());
  views::View::ConvertRectToScreen(browser_view->toolbar(), &bounds);
  bounds = gfx::Rect(
      bounds.x() +
          std::max(0, bounds.width() / 2 - kDefaultHatsDialogWidth / 2),
      bounds.bottom() - views::BubbleBorder::GetBorderAndShadowInsets().top(),
      kDefaultHatsDialogWidth, kDefaultHatsDialogHeight);
  gfx::NativeWindow native_window = chrome::CreateWebDialogWithBounds(
      browser_view->GetWidget()->GetNativeView(), off_the_record_profile(),
      this, bounds,
      /*show=*/false);
  preloading_widget_ = views::Widget::GetWidgetForNativeWindow(native_window);

  // Start the loading timer once it is created.
  loading_timer_.Start(FROM_HERE, ContentLoadingTimeout(),
                       base::BindOnce(&HatsWebDialog::OnLoadTimedOut,
                                      weak_factory_.GetWeakPtr()));
}

void HatsWebDialog::OnOriginalProfileDestroyed(Profile* profile) {
  if (otr_profile_registration_ &&
      profile == otr_profile_registration_->profile())
    otr_profile_registration_.reset();
}

void HatsWebDialog::Show(views::Widget* widget, bool accept) {
  if (accept) {
    if (widget)
      widget->Show();
    return;
  }

  // Delete this dialog.
  widget->Close();
}
