// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/hats/hats_next_web_dialog.h"

#include "chrome/browser/ui/browser_dialogs.h"

#include "base/util/values/values_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/hats/hats_bubble_view.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/pref_names.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "net/base/url_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

// A delegate used to intercept the creation of new WebContents by the HaTS
// Next dialog.
class HatsNextWebDialog::WebContentsDelegate
    : public content::WebContentsDelegate {
 public:
  explicit WebContentsDelegate(Browser* browser, HatsNextWebDialog* dialog)
      : browser_(browser), dialog_(dialog) {}

  bool IsWebContentsCreationOverridden(
      content::SiteInstance* source_site_instance,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url) override {
    return true;
  }

  content::WebContents* CreateCustomWebContents(
      content::RenderFrameHost* opener,
      content::SiteInstance* source_site_instance,
      bool is_new_browsing_instance,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url,
      const std::string& partition_id,
      content::SessionStorageNamespace* session_storage_namespace) override {
    // The HaTS Next WebDialog runs with a non-primary OTR profile. This profile
    // cannot open new browser windows, so they are instead opened in the
    // regular browser that initiated the HaTS survey.
    browser_->OpenURL(
        content::OpenURLParams(target_url, content::Referrer(),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui::PAGE_TRANSITION_LINK, false));
    return nullptr;
  }

  void SetContentsBounds(content::WebContents* source,
                         const gfx::Rect& bounds) override {
    // Check that the provided bounds do not exceed the dummy window size
    // provided to the HaTS library by the wrapper website. These are defined
    // in the website source at google3/chrome/hats/website/www/index.html.
    if (bounds.width() > 800 || bounds.height() > 600) {
      LOG(ERROR) << "Desired dimensions provided by contents exceed maximum"
                 << "allowable.";
      dialog_->CloseWidget();
      return;
    }
    dialog_->UpdateWidgetSize(bounds.size());
  }

 private:
  Browser* browser_;
  HatsNextWebDialog* dialog_;
};

// A thin wrapper that forwards the reference part of the URL associated with
// navigation events to the enclosing web dialog.
class HatsNextWebDialog::WebContentsObserver
    : public content::WebContentsObserver {
 public:
  WebContentsObserver(content::WebContents* contents, HatsNextWebDialog* dialog)
      : content::WebContentsObserver(contents), dialog_(dialog) {}

  // content::WebContentsObserver overrides.
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->IsSameDocument() &&
        navigation_handle->IsRendererInitiated()) {
      dialog_->OnSurveyStateUpdateReceived(navigation_handle->GetURL().ref());
    }
  }

 private:
  HatsNextWebDialog* dialog_;
};

HatsNextWebDialog::HatsNextWebDialog(Browser* browser,
                                     const std::string& trigger_id)
    : HatsNextWebDialog(
          browser,
          trigger_id,
          GURL("https://storage.googleapis.com/chrome_hats/index.html"),
          base::TimeDelta::FromSeconds(10)) {}

ui::ModalType HatsNextWebDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_NONE;
}

base::string16 HatsNextWebDialog::GetDialogTitle() const {
  return base::string16();
}

GURL HatsNextWebDialog::GetDialogContentURL() const {
  GURL param_url =
      net::AppendQueryParameter(hats_survey_url_, "trigger_id", trigger_id_);
  if (base::FeatureList::IsEnabled(
          features::kHappinessTrackingSurveysForDesktopDemo)) {
    param_url = net::AppendQueryParameter(param_url, "enable_testing", "true");
  }
  return param_url;
}

void HatsNextWebDialog::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {}

void HatsNextWebDialog::GetDialogSize(gfx::Size* size) const {}

std::string HatsNextWebDialog::GetDialogArgs() const {
  return std::string();
}

void HatsNextWebDialog::OnDialogClosed(const std::string& json_retval) {}

void HatsNextWebDialog::OnCloseContents(content::WebContents* source,
                                        bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool HatsNextWebDialog::ShouldShowCloseButton() const {
  return false;
}

bool HatsNextWebDialog::ShouldShowDialogTitle() const {
  return false;
}

bool HatsNextWebDialog::HandleContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  return true;
}
ui::WebDialogDelegate::FrameKind HatsNextWebDialog::GetWebDialogFrameKind()
    const {
  return ui::WebDialogDelegate::FrameKind::kDialog;
}

gfx::Size HatsNextWebDialog::CalculatePreferredSize() const {
  return size_;
}

void HatsNextWebDialog::OnProfileWillBeDestroyed(Profile* profile) {
  DCHECK_EQ(profile, otr_profile_);
  otr_profile_ = nullptr;
}

HatsNextWebDialog::HatsNextWebDialog(Browser* browser,
                                     const std::string& trigger_id,
                                     const GURL& hats_survey_url,
                                     const base::TimeDelta& timeout)
    : BubbleDialogDelegateView(BrowserView::GetBrowserViewForBrowser(browser)
                                   ->toolbar_button_provider()
                                   ->GetAppMenuButton(),
                               views::BubbleBorder::TOP_RIGHT),
      otr_profile_(browser->profile()->GetOffTheRecordProfile(
          Profile::OTRProfileID::CreateUnique("HaTSNext:WebDialog"))),
      browser_(browser),
      trigger_id_(trigger_id),
      hats_survey_url_(hats_survey_url),
      timeout_(timeout) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  otr_profile_->AddObserver(this);
  set_can_resize(false);
  set_close_on_deactivate(false);

  SetButtons(ui::DIALOG_BUTTON_NONE);

  SetLayoutManager(std::make_unique<views::FillLayout>());
  web_view_ = AddChildView(std::make_unique<views::WebDialogView>(
      otr_profile_, this, std::make_unique<ChromeWebContentsHandler>()));
  set_margins(gfx::Insets());
  widget_ = views::BubbleDialogDelegateView::CreateBubble(this);

  web_contents_observer_ =
      std::make_unique<WebContentsObserver>(web_view_->web_contents(), this);

  web_contents_delegate_ =
      std::make_unique<WebContentsDelegate>(browser_, this);
  web_view_->web_contents()->SetDelegate(web_contents_delegate_.get());

  loading_timer_.Start(FROM_HERE, timeout_,
                       base::BindOnce(&HatsNextWebDialog::CloseWidget,
                                      weak_factory_.GetWeakPtr()));
}

HatsNextWebDialog::~HatsNextWebDialog() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (otr_profile_) {
    otr_profile_->RemoveObserver(this);
    ProfileDestroyer::DestroyProfileWhenAppropriate(otr_profile_);
  }
  auto* service = HatsServiceFactory::GetForProfile(browser_->profile(), false);
  DCHECK(service);
  service->HatsNextDialogClosed();

  // Explicitly clear the delegate to ensure it is not invalid between now and
  // when the web contents is destroyed in the base class.
  web_view_->web_contents()->SetDelegate(nullptr);
}

void HatsNextWebDialog::OnSurveyStateUpdateReceived(std::string state) {
  loading_timer_.AbandonAndStop();

  if (state == "loaded") {
    // Record that the survey was shown, and display the widget.
    auto* service =
        HatsServiceFactory::GetForProfile(browser_->profile(), false);
    DCHECK(service);
    service->RecordSurveyAsShown(trigger_id_);
    ShowWidget();
  } else if (state == "close") {
    CloseWidget();
  } else {
    LOG(ERROR) << "Unknown state provided in URL fragment by HaTS survey:"
               << state;
    CloseWidget();
  }
}

void HatsNextWebDialog::SetHatsSurveyURLforTesting(GURL url) {
  hats_survey_url_ = url;
}

void HatsNextWebDialog::ShowWidget() {
  widget_->Show();
}

void HatsNextWebDialog::CloseWidget() {
  widget_->Close();
}

void HatsNextWebDialog::UpdateWidgetSize(gfx::Size size) {
  size_ = size;
  SizeToContents();
}

bool HatsNextWebDialog::IsWaitingForSurveyForTesting() {
  return loading_timer_.IsRunning();
}
